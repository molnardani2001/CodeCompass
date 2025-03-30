#include <parser/sourcemanager.h>
#include <util/odbtransaction.h>

#include <boost/filesystem.hpp>
#include <sstream>
#include <regex>

#include "valueanalyzer.h"

namespace cc
{
namespace parser
{

namespace fs = boost::filesystem;
constexpr int PRODUCE = 1;
constexpr int CONSUME = 0;

std::unordered_set<model::DependencyEdgeId> ValueAnalyzer::_edgeCache;
std::vector<model::Microservice> ValueAnalyzer::_microserviceCache;
std::mutex ValueAnalyzer::_edgeCacheMutex;
std::vector<model::Service> ValueAnalyzer::_serviceCache;
std::vector<model::KafkaTopic> ValueAnalyzer::_kafkaTopicCache;
std::map<model::HelmTemplateId,std::pair<std::vector<model::MicroserviceId>,std::vector<model::MicroserviceId>>> ValueAnalyzer::_kafkaRelations;
std::vector<model::Chart> ValueAnalyzer::_chartCache;

ValueAnalyzer::ValueAnalyzer(
  ParserContext& ctx_,
  std::map<std::string, YAML::Node>& fileAstCache_,
  std::uint64_t templateIdCounter)
  : _templateCounter(templateIdCounter), _ctx(ctx_), _fileAstCache(fileAstCache_)
{
  std::lock_guard<std::mutex> cacheLock(_edgeCacheMutex);

  if (_edgeCache.empty())
  {
    util::OdbTransaction{_ctx.db}([this]
    {
      for (const model::DependencyEdge& edge : _ctx.db->query<model::DependencyEdge>())
      {
        _edgeCache.insert(edge.id);
      }
    });
  }

  if(_chartCache.empty())
  {
    util::OdbTransaction{_ctx.db}([this]
    {
      for (const model::Chart& chart: _ctx.db->query<model::Chart>())
      {
        _chartCache.push_back(chart);
      }
    });
  }

  // Microservice components
  if (_microserviceCache.empty())
  {
    util::OdbTransaction{_ctx.db}([this]
    {
      for (const model::Microservice& service : _ctx.db->query<model::Microservice>())
      {
        _microserviceCache.push_back(service);
      }
    });
  }

  // Stream-platform connection between microservices is based on kafka-topics and kafka-users
  if(_kafkaTopicCache.empty())
  {
    util::OdbTransaction{_ctx.db}([this]
    {
      for (const model::KafkaTopic& topic : _ctx.db->query<model::KafkaTopic>())
      {
        _kafkaTopicCache.push_back(topic);
      }
    });
  }

  util::OdbTransaction{_ctx.db}([this]{
    for(const model::KafkaUser& kafkaUser : _ctx.db->query<model::KafkaUser>())
    {
      auto chartIt = std::find_if(_chartCache.begin(), _chartCache.end(),[&](const model::Chart& chart)
      {
        return chart.chartId == kafkaUser.depends;
      });

      if(chartIt != _chartCache.end())
      {
        processKafkaUserTopics(kafkaUser.consumeTopics, chartIt->microservice, CONSUME);
        processKafkaUserTopics(kafkaUser.produceTopics, chartIt->microservice, PRODUCE);
      }
      //_kafkaUserCache.push_back(kafkaUser);
    }
  });

  // On a cluster the hostname is based on service names, any communication between microservices
  // that uses protocol which needs hostname is based on services
  if(_serviceCache.empty())
  {
    util::OdbTransaction{_ctx.db}([this]
    {
      for (const model::Service& service : _ctx.db->query<model::Service>())
      {
        _serviceCache.push_back(service);
      }
    });
  }

}

ValueAnalyzer::~ValueAnalyzer()
{
  _ctx.srcMgr.persistFiles();

  (util::OdbTransaction(_ctx.db))([this]{
    for (model::HelmTemplate& helmTemplate : _newTemplates)
      _ctx.db->persist(helmTemplate);

    util::persistAll(_newEdges, _ctx.db);
  });
}

void ValueAnalyzer::init()
{
  (util::OdbTransaction(_ctx.db))([this]{
    std::for_each(_fileAstCache.begin(), _fileAstCache.end(),
    [&, this](std::pair<std::string, YAML::Node> pair)
    {
      auto filePtr = _ctx.db->query_one<model::File>(odb::query<model::File>::path == pair.first);
      filePtr->parent.load();
      fs::path potentialChartPath1 = fs::path(filePtr->parent->path) / "Chart.yml";
      fs::path potentialChartPath2 = fs::path(filePtr->parent->path) / "Chart.yaml";
      auto chartFilePtr = _ctx.db->query_one<model::File>(
        odb::query<model::File>::path == potentialChartPath1.string() ||
          odb::query<model::File>::path == potentialChartPath2.string());
      auto chartIt = std::find_if(
        _chartCache.begin(),
        _chartCache.end(),
        [&](model::Chart& chart)
        {
          return chart.file == chartFilePtr->id;
        });

      if (chartIt != _chartCache.end())
      {
        auto microserviceIt = std::find_if(
          _microserviceCache.begin(),
          _microserviceCache.end(),
          [&](model::Microservice& microservice)
          {
            return microservice.microserviceId == chartIt->microservice;
          });
        visitKeyValuePairs(pair.second, *microserviceIt, filePtr);
      }
    });

    std::for_each(
      _kafkaRelations.begin(),
      _kafkaRelations.end(),
      [&](const std::pair<
                    model::HelmTemplateId,
                    std::pair<std::vector<model::MicroserviceId>, std::vector<model::MicroserviceId>>>& item)
    {
      // On a specific topic, every consumer has dependency on the producer
      std::for_each(
        item.second.first.begin(),
        item.second.first.end(),
        [&](model::MicroserviceId consumerMicroserviceId)
        {
          std::for_each(
            item.second.second.begin(),
            item.second.second.end(),
            [&](model::MicroserviceId producerMicroserviceId)
            {
              // Collect only as dependency if the 2 microservices are not the same
              // (from communication point of view internal topics are not really important)
              if(consumerMicroserviceId != producerMicroserviceId)
                addEdge(consumerMicroserviceId, producerMicroserviceId, item.first, "STREAMING");
            });
        });
    });
  });
}

void ValueAnalyzer::processKafkaUserTopics(const std::string& topics, model::MicroserviceId microserviceId_, int actor)
{
  std::stringstream consumeTopicsStream(topics);
  std::string segment;

  while(std::getline(consumeTopicsStream, segment, ';'))
  {
    std::stringstream segmentStream(segment);
    std::string patternType, topic;

    if(std::getline(segmentStream, patternType, ':') &&
       std::getline(segmentStream, topic))
    {
      std::vector<model::HelmTemplateId> kafkaTopicIds;
      if("literal" == patternType)
      {
        auto kafkaTopicIt = std::find_if(_kafkaTopicCache.begin(), _kafkaTopicCache.end(),[&](const model::KafkaTopic& t)
        {
          return t.topicName == topic;
        });

        if(kafkaTopicIt != _kafkaTopicCache.end())
        {
          kafkaTopicIds.push_back(kafkaTopicIt->id);
        }
      } else if ("prefix" == patternType)
      {
        std::for_each(
          _kafkaTopicCache.begin(),
          _kafkaTopicCache.end(),
          [&](const model::KafkaTopic& t)
        {
          if(startsWith(t.topicName, topic))
          {
            kafkaTopicIds.push_back(t.id);
          }
        });
      }

      std::for_each(kafkaTopicIds.begin(), kafkaTopicIds.end(), [&](model::HelmTemplateId helmTemplateId)
      {
        auto relationIt = _kafkaRelations.find(helmTemplateId);
        switch(actor)
        {
          case CONSUME:
            if(relationIt != _kafkaRelations.end())
            {
              _kafkaRelations[helmTemplateId].first.push_back(microserviceId_);
            } else
            {
              _kafkaRelations.insert({helmTemplateId, {{microserviceId_},{}}});
            }
            break;

          case PRODUCE:
            if(relationIt != _kafkaRelations.end())
            {
              _kafkaRelations[helmTemplateId].second.push_back(microserviceId_);
            } else
            {
              _kafkaRelations.insert({helmTemplateId, {{},{microserviceId_}}});
            }
            break;

          default:
            /* DO NOTHING! */
            break;
        }
      });
    }
  }
}

bool ValueAnalyzer::visitKeyValuePairs(
  YAML::Node& currentNode_,
  model::Microservice& service_,
  const model::FilePtr& file_)
{
  for (auto it = currentNode_.begin(); it != currentNode_.end(); ++it)
  {
    if (it->second.IsDefined() && !it->second.IsScalar())
      visitKeyValuePairs(it->second, service_, file_);
    else
    {
      std::string currentValue(YAML::Dump(it->second));

      // If a microservice has another microservice's Service name
      // in its values.yaml file it means the microservice needs to know
      // the other microservice's hostname on the Cluster, probably there is a communication between them
      auto serviceIter = std::find_if(
        _serviceCache.begin(),
        _serviceCache.end(),
        [&](const model::Service& service) {
          return currentValue == service.name || isValuePossibleHostname(currentValue, service.name);
      });

      if (serviceIter != _serviceCache.end())
      {
        auto otherChartIt = std::find_if(
        _chartCache.begin(),
        _chartCache.end(),
        [&](const model::Chart& chart)
        {
          return chart.chartId == serviceIter->depends;
        });

        if (otherChartIt != _chartCache.end() && otherChartIt->microservice != service_.microserviceId)
          addEdge(service_.microserviceId, otherChartIt->microservice, serviceIter->id, "REST");
      }
    }
  }
}

void ValueAnalyzer::addHelmTemplate(
  model::HelmTemplate& helmTemplate_)
{
  auto it = std::find_if(_newTemplates.begin(), _newTemplates.end(),
    [&](auto& helm)
    {
     return helm.id == helmTemplate_.id;
    });

  if (it == _newTemplates.end())
    _newTemplates.push_back(helmTemplate_);
}

void ValueAnalyzer::addEdge(
  model::MicroserviceId from_,
  model::MicroserviceId to_,
  model::HelmTemplateId connect_,
  std::string type_)
{
  static std::mutex m;
  std::lock_guard<std::mutex> guard(m);

  model::DependencyEdgePtr edge = std::make_shared<model::DependencyEdge>();

  edge->from = std::make_shared<model::Microservice>();
  edge->from->microserviceId = from_;
  edge->to = std::make_shared<model::Microservice>();
  edge->to->microserviceId = to_;

  edge->connection = std::make_shared<model::HelmTemplate>();
  edge->connection->id = connect_;

  edge->type = std::move(type_);
  edge->helperId = ++_templateCounter;
  edge->id = model::createIdentifier(*edge);

  if (_edgeCache.insert(edge->id).second)
  {
    _newEdges.push_back(edge);
  }
}

std::shared_ptr<model::HelmTemplate> ValueAnalyzer::findHelmTemplate(
  model::HelmTemplateId helmTemplateId)
{
  return _ctx.db->query_one<model::HelmTemplate>(
    odb::query<model::HelmTemplate>::id == helmTemplateId);
}

bool ValueAnalyzer::startsWith(
  const std::string& str,
  const std::string& prefix)
{
  return prefix.length() <= str.length() &&
    std::equal(prefix.begin(), prefix.end(), str.begin());
}

bool ValueAnalyzer::isValuePossibleHostname(
  const std::string& value,
  const std::string& serviceName)
{
  if (value.find(serviceName) == std::string::npos) {
    return false;
  }

  std::vector<std::string> protocols = {
    "http://", "https://", "grpc://", "kafka://", "redis://", "amqp://",
    "postgres://", "mysql://", "mongodb://", "etcd://", "nats://", "ssl://"
  };

  for (const auto& protocol : protocols) {
    if (value.find(protocol) != std::string::npos) {
      return true;
    }
  }

  // kubernetes services can be accessed as `service.namespace.svc.cluster.local`
  std::regex k8s_service_regex(R"(([\w-]+)\.([\w-]+)\.svc(\.cluster\.local)?)");
  if (std::regex_search(value, k8s_service_regex)) {
    return true;
  }

  return false;
}

}
}