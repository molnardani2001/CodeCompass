#include <parser/sourcemanager.h>
#include <util/odbtransaction.h>

#include "templateanalyzer.h"

namespace cc
{
namespace parser
{

std::unordered_set<model::MicroserviceEdgeId> TemplateAnalyzer::_edgeCache;
std::vector<model::Microservice> TemplateAnalyzer::_microserviceCache;
std::mutex TemplateAnalyzer::_edgeCacheMutex;
std::vector<model::Kafkatopic> TemplateAnalyzer::_kafkaTopicCache;

TemplateAnalyzer::TemplateAnalyzer(
  cc::parser::ParserContext& ctx_,
  std::map<std::string, YAML::Node>& fileAstCache_)
  : _ctx(ctx_), _fileAstCache(fileAstCache_)
{
  fillDependencyPairsMap();
  fillResourceTypePairsMap();

  std::lock_guard<std::mutex> cacheLock(_edgeCacheMutex);
  if (_edgeCache.empty())
  {
    util::OdbTransaction{_ctx.db}([this]
    {
      for (const model::MicroserviceEdge& edge : _ctx.db->query<model::MicroserviceEdge>())
      {
        _edgeCache.insert(edge.id);
      }
    });
  }

  if (_microserviceCache.empty())
  {
    util::OdbTransaction{_ctx.db}([this]
    {
      for (const model::Microservice& service : _ctx.db->query<model::Microservice>(
        odb::query<model::Microservice>::type == model::Microservice::ServiceType::INTEGRATION ||
        odb::query<model::Microservice>::type == model::Microservice::ServiceType::CENTRAL))
      {
        _microserviceCache.push_back(service);
      }
    });

    for (auto it = _microserviceCache.begin(); it != _microserviceCache.end(); ++it)
      LOG(info) << it->serviceId << ", " << it->name;
  }

  templateCounter = 0;
}

TemplateAnalyzer::~TemplateAnalyzer()
{
  (util::OdbTransaction(_ctx.db))([this]{
    for (model::HelmTemplate& helmTemplate : _newTemplates)
    {
      LOG(warning) << helmTemplate.id << " " << helmTemplate.name;
      //_ctx.db->persist(helmTemplate);
    }

    for (model::MSResource& msResource : _msResources)
      _ctx.db->persist(msResource);

    util::persistAll(_newEdges, _ctx.db);
  });
}

void TemplateAnalyzer::init()
{
  (util::OdbTransaction(_ctx.db))([this]{
    std::for_each(_fileAstCache.begin(), _fileAstCache.end(),
    [&, this](std::pair<std::string, YAML::Node> pair)
    {
      /*std::stringstream path(pair.first);
      std::string segment;
      std::vector<std::string> segList;

      while(std::getline(path, segment, '/'))
      {
        segList.push_back(segment);
      }

      auto productIter =
        std::find_if(_microserviceCache.begin(), _microserviceCache.end(),
          [&](model::Microservice& service)
          {
            return service.type == model::Microservice::ServiceType::PRODUCT;
          });*/

      auto currentService = std::find_if(_microserviceCache.begin(),
        _microserviceCache.end(),
        [&](model::Microservice& service)
        {
          return pair.first.find(service.name) != std::string::npos;
        });

      if (currentService != _microserviceCache.end())
        visitKeyValuePairs(pair.first, pair.second, *currentService);
    });

    for (const model::Microservice& service : _ctx.db->query<model::Microservice>(
      odb::query<model::Microservice>::type == model::Microservice::ServiceType::PRODUCT))
    {
      _microserviceCache.push_back(service);
    }

    std::for_each(_fileAstCache.begin(), _fileAstCache.end(),
      [&, this](std::pair<std::string, YAML::Node> pair)
      {
        auto currentService = std::find_if(_microserviceCache.begin(),
         _microserviceCache.end(),
         [&](model::Microservice& service)
         {
            return service.type == model::Microservice::ServiceType::PRODUCT &&
                   pair.first.find("/charts/") == std::string::npos;
         });

        if (currentService != _microserviceCache.end())
          visitKeyValuePairs(pair.first, pair.second, *currentService);
      });
  });
}

bool TemplateAnalyzer::visitKeyValuePairs(
  std::string path_,
  YAML::Node& currentNode_,
  model::Microservice& service_)
{
  typedef model::HelmTemplate::DependencyType DependencyType;

  if (!currentNode_["metadata"] ||
      !currentNode_["metadata"].IsMap() ||
      !currentNode_["kind"] ||
      !currentNode_["metadata"]["name"])
    return false;

  auto typeIter = _dependencyPairs.find(YAML::Dump(currentNode_["kind"]));
  auto type = typeIter->second;

  if (typeIter == _dependencyPairs.end())
  {
    if (YAML::Dump(currentNode_["kind"]).find("Certificate") != std::string::npos ||
        YAML::Dump(currentNode_["kind"]).find("InternalUserCA") != std::string::npos)
    {
      type = DependencyType::CERTIFICATE;
    }
    else
    {
      auto volumesNode = findKey("volumes", currentNode_);
      if (volumesNode.IsDefined())
        type = DependencyType::MOUNT;
      else
        return false;
    }
  }

  switch (type)
  {
    case DependencyType::SERVICE:
      processServiceDeps(path_, currentNode_, service_);
      break;
    case DependencyType::MOUNT:
      processMountDeps(path_, currentNode_, service_);
      break;
    case DependencyType::CERTIFICATE:
      processCertificateDeps(path_, currentNode_, service_);
      break;
    case DependencyType::KAFKATOPIC:
      processKafkaTopicDeps(path_, currentNode_, service_);
    case DependencyType::RESOURCE:
    case DependencyType::OTHER:
      break;
  }

  processResources(path_, currentNode_, service_);
  processStorageResources(path_, currentNode_, service_);

  return true;
}

void TemplateAnalyzer::processServiceDeps(
  const std::string& path_,
  YAML::Node& currentFile_,
  model::Microservice& service_)
{
  /* --- Process Service templates --- */

  // Find MS in database.
  auto serviceIter = std::find_if(_microserviceCache.begin(), _microserviceCache.end(),
    [&](const model::Microservice& service)
    {
      //LOG(info) << service.name << ", " << YAML::Dump(currentFile_["metadata"]["name"]);;
      return service.name == YAML::Dump(currentFile_["metadata"]["name"]);
    });
  //LOG(warning) << "service deps: " << serviceIter->name;

  // Persist template data to db.
  model::HelmTemplate helmTemplate;// = std::make_shared<model::HelmTemplate>();
  helmTemplate.dependencyType = model::HelmTemplate::DependencyType::SERVICE;

  auto filePtr = _ctx.db->query_one<model::File>(odb::query<model::File>::path == path_);
  helmTemplate.file = filePtr->id;
  helmTemplate.kind = YAML::Dump(currentFile_["kind"]);
  helmTemplate.name = YAML::Dump(currentFile_["metadata"]["name"]);

  // If the MS is not present in the db,
  // it is an external / central MS,
  // and should be added to the db.
  //LOG(info) << serviceIter->serviceId << ", " << serviceIter->name;
  if (serviceIter == _microserviceCache.end())
  {
    model::Microservice externalService;
    externalService.name = YAML::Dump(currentFile_["metadata"]["name"]);
    externalService.type = model::Microservice::ServiceType::CENTRAL;
    externalService.file = filePtr->id;
    externalService.serviceId = createIdentifier(externalService);
    externalService.version = YAML::Dump(currentFile_["metadata"]["labels"]["app.kubernetes.io\/version"]);
    _microserviceCache.push_back(externalService);
    _ctx.db->persist(externalService);

    helmTemplate.depends = externalService.serviceId;
  }
  else
  {
    helmTemplate.depends = serviceIter->serviceId;
  }

  helmTemplate.id = createIdentifier(helmTemplate);
  addHelmTemplate(helmTemplate);
  addEdge(service_.serviceId, helmTemplate.depends, helmTemplate.id, helmTemplate.kind);
}

void TemplateAnalyzer::processMountDeps(
  const std::string& path_,
  YAML::Node& currentFile_,
  model::Microservice& service_)
{
  /* --- Processing ConfigMap templates --- */

  //auto volumesNode = findKey("volumes", currentFile_);
  std::vector<YAML::Node> nodes;
  findKeys("volumes", nodes, currentFile_);

  for (auto volumesNode = nodes.begin(); volumesNode != nodes.end(); ++volumesNode)
  {
    if (!volumesNode->IsDefined())
      return;

    for (auto volume = volumesNode->begin(); volume != volumesNode->end(); ++volume)
    {
      if ((*volume)["configMap"] && (*volume)["configMap"]["name"])
      {
        model::HelmTemplate helmTemplate;
        helmTemplate.dependencyType = model::HelmTemplate::DependencyType::MOUNT;
        helmTemplate.kind = "ConfigMap";
        auto filePtr = _ctx.db->query_one<model::File>(odb::query<model::File>::path == path_);
        helmTemplate.file = filePtr->id;
        helmTemplate.name = YAML::Dump((*volume)["configMap"]["name"]);

        auto serviceIter = std::find_if(_microserviceCache.begin(), _microserviceCache.end(),
          [&](const model::Microservice &service)
          {
            return (YAML::Dump((*volume)["configMap"]["name"])).find(service.name) !=
                   std::string::npos;
          });

        if (serviceIter == _microserviceCache.end())
        {
          helmTemplate.depends = -1;
          helmTemplate.id = createIdentifier(helmTemplate);
        }
        else
        {
          helmTemplate.depends = serviceIter->serviceId;
          helmTemplate.id = createIdentifier(helmTemplate);
          addEdge(service_.serviceId, helmTemplate.depends, helmTemplate.id, helmTemplate.kind);
        }

        addHelmTemplate(helmTemplate);
      }
      else if ((*volume)["secret"] && (*volume)["secret"]["secretName"])
      {
        model::HelmTemplate helmTemplate;
        helmTemplate.dependencyType = model::HelmTemplate::DependencyType::MOUNT;
        helmTemplate.kind = "Secret";
        auto filePtr = _ctx.db->query_one<model::File>(odb::query<model::File>::path == path_);
        helmTemplate.file = filePtr->id;
        helmTemplate.name = YAML::Dump((*volume)["secret"]["secretName"]);

        auto serviceIter = std::find_if(_microserviceCache.begin(), _microserviceCache.end(),
          [&](const model::Microservice &service)
          {
            return (YAML::Dump((*volume)["secret"]["secretName"])).find(service.name) !=
                   std::string::npos;
          });

        if (serviceIter == _microserviceCache.end())
        {
          /*auto dependentServiceIt = std::find_if(_newTemplates.begin(), _newTemplates.end(),
            [&](const model::HelmTemplate &dependentService)
            {
             return (YAML::Dump((*volume)["secret"]["secretName"])).find(service.name) !=
                    std::string::npos;
            });*/
          helmTemplate.depends = -1;
          helmTemplate.id = createIdentifier(helmTemplate);
        }
        else
        {
          helmTemplate.depends = serviceIter->serviceId;
          helmTemplate.id = createIdentifier(helmTemplate);
          addEdge(service_.serviceId, helmTemplate.depends, helmTemplate.id, helmTemplate.kind);
        }

        addHelmTemplate(helmTemplate);
      }
    }
  }
}

void TemplateAnalyzer::processCertificateDeps(
  const std::string& path_,
  YAML::Node& currentFile_,
  model::Microservice& service_)
{
  model::HelmTemplate helmTemplate;
  helmTemplate.dependencyType = model::HelmTemplate::DependencyType::CERTIFICATE;
  helmTemplate.kind = YAML::Dump(currentFile_["kind"]);
  auto filePtr = _ctx.db->query_one<model::File>(odb::query<model::File>::path == path_);
  helmTemplate.file = filePtr->id;
  helmTemplate.name = YAML::Dump(currentFile_["metadata"]["name"]);

  auto keyName = findKey("generatedSecretName", currentFile_);

  auto serviceIter = std::find_if(_microserviceCache.begin(), _microserviceCache.end(),
    [&](const model::Microservice& service)
    {
      return (YAML::Dump(keyName)).find(service.name) != std::string::npos;
    });

  if (serviceIter == _microserviceCache.end())
  {
    helmTemplate.depends = -1;
    helmTemplate.id = createIdentifier(helmTemplate);
  }
  else
  {
    helmTemplate.depends = serviceIter->serviceId;
    helmTemplate.id = createIdentifier(helmTemplate);
    addEdge(service_.serviceId, helmTemplate.depends, helmTemplate.id, helmTemplate.kind);
  }

  addHelmTemplate(helmTemplate);

  model::HelmTemplate secretTemplate;
  secretTemplate.dependencyType = model::HelmTemplate::DependencyType::CERTIFICATE;
  secretTemplate.kind = "Secret";
  secretTemplate.file = filePtr->id;
  secretTemplate.name = YAML::Dump(keyName);

  if (serviceIter == _microserviceCache.end())
  {
    secretTemplate.depends = -1;
    secretTemplate.id = createIdentifier(secretTemplate);
  }
  else
  {
    secretTemplate.depends = serviceIter->serviceId;
    secretTemplate.id = createIdentifier(secretTemplate);
    addEdge(service_.serviceId, secretTemplate.depends, secretTemplate.id, secretTemplate.kind);
  }

  addHelmTemplate(secretTemplate);
}

void TemplateAnalyzer::processKafkaTopicDeps(
  const std::string& path_,
  YAML::Node& currentFile_,
  model::Microservice& service_)
{
  /* --- Process KafkaTopic templates --- */

  // Find MS in database who responsible for creating the KafkaTopic.
  auto serviceIter = std::find_if(_microserviceCache.begin(), _microserviceCache.end(),
    [&](const model::Microservice& service)
    {
      return path_.find(service.name) != std::string::npos;
    });

  // Persist template data to db.
  model::HelmTemplate helmTemplate;
  helmTemplate.dependencyType = model::HelmTemplate::DependencyType::KAFKATOPIC;
  auto filePtr = _ctx.db->query_one<model::File>(odb::query<model::File>::path == path_);
  helmTemplate.file = filePtr->id;
  helmTemplate.kind = YAML::Dump(currentFile_["kind"]);
  helmTemplate.name = YAML::Dump(currentFile_["metadata"]["name"]);

  // If the MS is not present in the db,
  // it is an external / central MS,
  // and should be added to the db.
  if (serviceIter == _microserviceCache.end())
  {
    model::Microservice externalService;
    externalService.name = "placeholder-external-service";
    externalService.type = model::Microservice::ServiceType::CENTRAL;
    externalService.file = filePtr->id;
    externalService.serviceId = createIdentifier(externalService);
    externalService.version = "placeholder-some-version";
    _microserviceCache.push_back(externalService);
    _ctx.db->persist(externalService);

    helmTemplate.depends = externalService.serviceId;
  }
  else
  {
    helmTemplate.depends = serviceIter->serviceId;
  }

  auto kafkaIter = std::find_if(_kafkaTopicCache.begin(), _kafkaTopicCache.end(),
    [&](const model::Kafkatopic& topic)
    {
      return topic.topicName == YAML::Dump(currentFile_["spec"]["topicName"]);
    });

  if (kafkaIter == _kafkaTopicCache.end())
  {
    model::Kafkatopic topic;
    topic.name = YAML::Dump(currentFile_["metadata"]["name"]);
    topic.topicName = YAML::Dump(currentFile_["spec"]["topicName"]);
    if (currentFile_["spec"]["replicas"] && currentFile_["spec"]["replicas"].IsScalar())
      topic.replicaCount = currentFile_["spec"]["replicas"].as<uint64_t>();
    else
      topic.replicaCount = 1;
    if (currentFile_["spec"]["partitions"] && currentFile_["spec"]["partitions"].IsScalar())
      topic.partitionCount = currentFile_["spec"]["partitions"].as<uint64_t>();
    else
      topic.partitionCount = 1;

    _kafkaTopicCache.push_back(topic);
  }

  helmTemplate.id = createIdentifier(helmTemplate);
  addHelmTemplate(helmTemplate);
  addEdge(service_.serviceId, helmTemplate.depends, helmTemplate.id, helmTemplate.kind);
}

void TemplateAnalyzer::processResources(
  const std::string& path_,
  YAML::Node& currentFile_,
  model::Microservice& service_)
{
  /* If the resource keys are not found, return. */
  YAML::Node resourcesKey = findKey("resources", currentFile_);
  if (!resourcesKey.IsDefined())
    return;

  YAML::Node requestsKey = findKey("requests", resourcesKey);
  if (!requestsKey.IsDefined())
    return;

  /* Collect the resources. */

  for (const auto& pair : requestsKey)
  {
    auto it = _msResourcePairs.find(YAML::Dump(pair.first));
    if (it == _msResourcePairs.end())
      return;

    model::MSResource resource;
    resource.type = it->second;
    resource.service = service_.serviceId;

    auto convertedAmount = convertUnit(YAML::Dump(pair.second), it->second);
    resource.amount = convertedAmount.first;
    resource.unit = convertedAmount.second;

    _msResources.push_back(resource);
  }
}

void TemplateAnalyzer::processStorageResources(
  const std::string& path_,
  YAML::Node& currentFile_,
  model::Microservice& service_)
{
  /* --- Collect storage resources --- */
  YAML::Node volumeClaimsKey = findKey("volumeClaimTemplates", currentFile_);
  if (!volumeClaimsKey.IsDefined())
    return;

  YAML::Node storageKey = findKey("storage", volumeClaimsKey);
  if (!storageKey.IsDefined())
  {
    LOG(info) << " storageKey not found";
    return;
  }

  auto it = _msResourcePairs.find("storage");
  if (it == _msResourcePairs.end())
    return;

  model::MSResource resource;
  resource.type = it->second;
  resource.service = service_.serviceId;

  LOG(info) << YAML::Dump(storageKey);
  auto convertedAmount = convertUnit(YAML::Dump(storageKey), it->second);
  resource.amount = convertedAmount.first;
  resource.unit = convertedAmount.second;

  _msResources.push_back(resource);
}

std::pair<float, std::string> TemplateAnalyzer::convertUnit(
  std::string amount_,
  model::MSResource::ResourceType type_)
{
  switch (type_)
  {
    case model::MSResource::ResourceType::CPU:
    {
      int m = amount_.find('m');
      if (m != std::string::npos)
      {
        amount_.erase(m, 1);
        return std::make_pair(std::stof(amount_) / 1000, "");
      }
      else
      {
        return std::make_pair(std::stof(amount_), "");
      }
    }

    case model::MSResource::ResourceType::MEMORY:
    case model::MSResource::ResourceType::STORAGE:
    {
      int m = amount_.find('M');
      if (m != std::string::npos)
      {
        amount_.erase(m, 2);
        LOG(info) << std::stof(amount_);
        return std::make_pair(std::stof(amount_) / 1024, "Gi");
      }

      int g = amount_.find('G');
      if (g != std::string::npos)
      {
        amount_.erase(g, 2);
        LOG(info) << std::stof(amount_);
        return std::make_pair(std::stof(amount_), "Gi");
      }
      break;
    }
  }

  return {};
}

YAML::Node TemplateAnalyzer::findKey(
  const std::string& key_,
  const YAML::Node& node_)
{
  switch (node_.Type())
  {
    case YAML::NodeType::Scalar:
    case YAML::NodeType::Null:
    case YAML::NodeType::Undefined:
      break;
    case YAML::NodeType::Sequence:
      for (auto elem : node_)
        if (elem.IsMap())
          return findKey(key_, elem);
      break;
    case YAML::NodeType::Map:
      if (node_[key_])
        return node_[key_];
      else
        for (auto iter = node_.begin(); iter != node_.end(); ++iter)
        {
          YAML::Node temp = findKey(key_, iter->second);
          if (temp.IsDefined())
            return temp;
        }
      break;
  }

  return YAML::Node(YAML::NodeType::Undefined);
}

std::vector<YAML::Node> TemplateAnalyzer::findKeys(
  const std::string& key_,
  std::vector<YAML::Node>& nodes_,
  YAML::Node& node_)
{
  switch (node_.Type())
  {
    case YAML::NodeType::Scalar:
    case YAML::NodeType::Null:
    case YAML::NodeType::Undefined:
      break;
    case YAML::NodeType::Sequence:
      for (auto elem : node_)
        if (elem.IsMap())
          findKeys(key_, nodes_, elem);
      break;
    case YAML::NodeType::Map:
      if (node_[key_])
      {
        nodes_.push_back(node_[key_]);
        //return findKeys(key_, nodes_, node_);
        //return nodes_;
      }
      else
        for (auto iter = node_.begin(); iter != node_.end(); ++iter)
        {
          findKeys(key_, nodes_, iter->second);
          //if (temp.IsDefined())
          //{
            //nodes_.push_back(temp);
            //return nodes_;
          //}
        }
      break;
  }

  return nodes_;
}

void TemplateAnalyzer::addHelmTemplate(model::HelmTemplate& helmTemplate_)
{
  auto it = std::find_if(_newTemplates.begin(), _newTemplates.end(),
    [&](auto& helm)
    {
        return helm.id == helmTemplate_.id;
    });

  if (it == _newTemplates.end())
  {
    _newTemplates.push_back(helmTemplate_);
    _ctx.db->persist(helmTemplate_);
  }
}

void TemplateAnalyzer::addEdge(
  const model::MicroserviceId& from_,
  const model::MicroserviceId& to_,
  const model::HelmTemplateId& connect_,
  std::string type_)
{
  static std::mutex m;
  std::lock_guard<std::mutex> guard(m);

  model::MicroserviceEdgePtr edge = std::make_shared<model::MicroserviceEdge>();

  edge->from = std::make_shared<model::Microservice>();
  edge->from->serviceId = from_;
  edge->to = std::make_shared<model::Microservice>();
  edge->to->serviceId = to_;

  edge->connection = std::make_shared<model::HelmTemplate>();
  edge->connection->id = connect_;

  edge->type = std::move(type_);
  edge->helperId = ++templateCounter;
  edge->id = model::createIdentifier(*edge);

  if (_edgeCache.insert(edge->id).second)
  {
    _newEdges.push_back(edge);
  }
}

int TemplateAnalyzer::LCSubStr(std::string& s1, std::string& s2, int m, int n)
{
  // Create a table to store
  // lengths of longest
  // common suffixes of substrings.
  // Note that LCSuff[i][j] contains
  // length of longest common suffix
  // of X[0..i-1] and Y[0..j-1].

  int LCSuff[m + 1][n + 1];
  int result = 0; // To store length of the
  // longest common substring

  /* Following steps build LCSuff[m+1][n+1] in
      bottom up fashion. */
  for (int i = 0; i <= m; i++)
  {
    for (int j = 0; j <= n; j++)
    {
      // The first row and first column
      // entries have no logical meaning,
      // they are used only for simplicity
      // of program
      if (i == 0 || j == 0)
        LCSuff[i][j] = 0;

      else if (s1[i - 1] == s2[j - 1]) {
        LCSuff[i][j] = LCSuff[i - 1][j - 1] + 1;
        result = std::max(result, LCSuff[i][j]);
      }
      else
        LCSuff[i][j] = 0;
    }
  }
  return result;
}

void TemplateAnalyzer::fillDependencyPairsMap()
{
  _dependencyPairs.insert({"Service", model::HelmTemplate::DependencyType::SERVICE});
  _dependencyPairs.insert({"ConfigMap", model::HelmTemplate::DependencyType::MOUNT});
  _dependencyPairs.insert({"Secret", model::HelmTemplate::DependencyType::MOUNT});
  _dependencyPairs.insert({"Certificate", model::HelmTemplate::DependencyType::CERTIFICATE});
  _dependencyPairs.insert({"VolumeClaim", model::HelmTemplate::DependencyType::RESOURCE});
  _dependencyPairs.insert({"KafkaTopic", model::HelmTemplate::DependencyType::KAFKATOPIC});
}

void TemplateAnalyzer::fillResourceTypePairsMap()
{
  _msResourcePairs.insert({"cpu", model::MSResource::ResourceType::CPU});
  _msResourcePairs.insert({"memory", model::MSResource::ResourceType::MEMORY});
  _msResourcePairs.insert({"storage", model::MSResource::ResourceType::STORAGE});
}

}
}