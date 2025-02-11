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
//std::vector<model::Kafkatopic> ValueAnalyzer::_kafkaTopicCache;

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

//  if (_microserviceCache.empty())
//  {
//    util::OdbTransaction{_ctx.db}([this]
//    {
//      for (const model::Microservice& service : _ctx.db->query<model::Microservice>(
//        odb::query<model::Microservice>::type == model::Microservice::ServiceType::INTEGRATION ||
//        odb::query<model::Microservice>::type == model::Microservice::ServiceType::CENTRAL))
//      {
//        _microserviceCache.push_back(service);
//      }
//    });
//  }
//
//  if(_kafkaTopicCache.empty())
//  {
//    util::OdbTransaction{_ctx.db}([this]
//    {
//      for (const model::Kafkatopic& topic : _ctx.db->query<model::Kafkatopic>())
//      {
//        _kafkaTopicCache.push_back(topic);
//      }
//    });
//  }

//  if(_serviceCache.empty())
//  {
//    util::OdbTransaction{_ctx.db}([this]
//    {
//      for (const model::Service& service : _ctx.db->query<model::Service>())
//      {
//        _serviceCache.push_back(service);
//      }
//    });
//  }
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
      auto currentService = std::find_if(_microserviceCache.begin(),
        _microserviceCache.end(),
        [&](model::Microservice& service)
        {
          return pair.first.find(service.name) != std::string::npos;
        });

      if (currentService != _microserviceCache.end())
        visitKeyValuePairs(pair.second, *currentService, filePtr);
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
      // in its values.yaml file it means
      auto serviceIter = std::find_if(_serviceCache.begin(),
        _serviceCache.end(),
        [&, this](const model::Service& other) {
          return currentValue == other.name;
      });

      if (serviceIter != _serviceCache.end())
      {
//        auto microserviceIter = std::find_if(_microserviceCache.begin(),
//          _microserviceCache.end(),
//          [&](const model::Microservice& other)
//          {
//            return serviceIter->microserviceId == other.microserviceId;
//          });

//        if ( microserviceIter != _microserviceCache.end())
//        {
//          model::HelmTemplate helmTemplate = std::move(*findHelmTemplate(serviceIter->helmTemplateId));
//          addEdge(service_.microserviceId, serviceIter->depends, helmTemplate.id, "Service");
//        }
      }

//      if (iter != _microserviceCache.end())
//      {
//        model::HelmTemplate helmTemplate;
//        helmTemplate.name = "";
//        helmTemplate.templateType = model::HelmTemplate::TemplateType::SERVICE;
//        helmTemplate.depends = service_.microserviceId;
//        helmTemplate.kind = "Service";
//        helmTemplate.file = file_->id;
//        helmTemplate.id = createIdentifier(helmTemplate);
//        addHelmTemplate(helmTemplate);
//       }

//      auto kafkaTopicIter = std::find_if(_kafkaTopicCache.begin(),
//        _kafkaTopicCache.end(),
//        [&](const model::Kafkatopic& topic)
//        {
//            return currentValue == topic.topicName;
//        });
//
//      if (kafkaTopicIter != _kafkaTopicCache.end())
//      {
//        model::HelmTemplate helmTemplate = std::move(*findHelmTemplate(kafkaTopicIter->helmTemplateId));
//        addEdge(service_.microserviceId, kafkaTopicIter->depends, helmTemplate.id, "Kafka-topic");
//      }
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