#include <parser/sourcemanager.h>
#include <util/odbtransaction.h>

#include <boost/filesystem.hpp>

#include "valueanalyzer.h"

namespace cc
{
namespace parser
{

namespace fs = boost::filesystem;

std::unordered_set<model::DependencyEdgeId> ValueAnalyzer::_edgeCache;
std::vector<model::Microservice> ValueAnalyzer::_microserviceCache;
std::mutex ValueAnalyzer::_edgeCacheMutex;
std::vector<model::Service> ValueAnalyzer::_serviceCache;
std::vector<model::KafkaTopic> ValueAnalyzer::_kafkaTopicCache;
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

  // Stream-platform connection between microservices is based on kafka-topics
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

  if(_chartCache.empty())
  {
    util::OdbTransaction{_ctx.db}([this]
    {
      for (const model::Chart& chart : _ctx.db->query<model::Chart>())
      {
        _chartCache.push_back(chart);
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

//    for (const model::Microservice& service : _ctx.db->query<model::Microservice>(
//      odb::query<model::Microservice>::type == model::Microservice::ServiceType::PRODUCT))
//    {
//      _microserviceCache.push_back(service);
//    }

//    std::for_each(_fileAstCache.begin(), _fileAstCache.end(),
//      [&, this](std::pair<std::string, YAML::Node> pair)
//      {
//        auto filePtr = _ctx.db->query_one<model::File>(odb::query<model::File>::path == pair.first);
//        auto currentService = std::find_if(_microserviceCache.begin(), _microserviceCache.end(),
//         [&](model::Microservice& service)
//         {
//           return service.type == model::Microservice::ServiceType::PRODUCT &&
//                  pair.first.find("/charts/") == std::string::npos;
//         });
//
//        if (currentService != _microserviceCache.end())
//          visitKeyValuePairs(pair.second, *currentService, filePtr);
//    });
  });
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
        [&](const model::Service& other) {
          return currentValue == other.name;
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

      auto kafkaTopicIter = std::find_if(_kafkaTopicCache.begin(),
        _kafkaTopicCache.end(),
        [&](const model::KafkaTopic& topic)
        {
            return currentValue == topic.topicName;
        });

      if (kafkaTopicIter != _kafkaTopicCache.end())
      {
        auto otherChartIt = std::find_if(
        _chartCache.begin(),
        _chartCache.end(),
        [&](const model::Chart& chart)
        {
          return chart.chartId == kafkaTopicIter->depends;
        });

        if (otherChartIt != _chartCache.end() && otherChartIt->microservice != service_.microserviceId)
          addEdge(service_.microserviceId, otherChartIt->microservice, kafkaTopicIter->id, "STREAMING");
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
  const model::MicroserviceId& from_,
  const model::MicroserviceId& to_,
  const model::HelmTemplateId& connect_,
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

}
}