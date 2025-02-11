#include <parser/sourcemanager.h>
#include <util/odbtransaction.h>

#include "templateanalyzer.h"

namespace cc
{
namespace parser
{

std::unordered_set<model::DependencyEdgeId> TemplateAnalyzer::_edgeCache;
std::unordered_set<model::HelmTemplateDependencyEdgeId> TemplateAnalyzer::_helmTemplateEdgeCache;
std::vector<model::Chart> TemplateAnalyzer::_chartCache;

std::vector<model::Microservice> TemplateAnalyzer::_microserviceCache;
std::mutex TemplateAnalyzer::_edgeCacheMutex;
//std::vector<model::Kafkatopic> TemplateAnalyzer::_kafkaTopicCache;
//std::vector<model::Service> TemplateAnalyzer::_serviceCache;
std::vector<model::HelmTemplate> TemplateAnalyzer::_helmTemplateCache;

TemplateAnalyzer::TemplateAnalyzer(
  cc::parser::ParserContext& ctx_,
  std::map<std::string, std::vector<YAML::Node>>& fileAstCache_,
  std::map<std::string, std::vector<std::pair<std::string, YAML::Node>>>& templateCache_)
  : _ctx(ctx_), _fileAstCache(fileAstCache_), _templateCache(templateCache_)
{
  fillDependencyPairsMap();
  fillResourceTypePairsMap();

  fillHelmTemplateHandlers();
  fillWorkloadHandlers();

  std::lock_guard<std::mutex> cacheLock(_edgeCacheMutex);
//  if (_edgeCache.empty())
//  {
//    util::OdbTransaction{_ctx.db}([this]
//    {
//      for (const model::DependencyEdge& edge : _ctx.db->query<model::DependencyEdge>())
//      {
//        _edgeCache.insert(edge.id);
//      }
//    });
//  }

  if (_microserviceCache.empty())
  {
    util::OdbTransaction{_ctx.db}([this]
    {
      for (const model::Microservice& service : _ctx.db->query<model::Microservice>())
      {
        _microserviceCache.push_back(service);
      }
    });

    for (auto & it : _microserviceCache)
      LOG(info) << it.microserviceId << ", " << it.name;
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

  templateCounter = 0;
}

void TemplateAnalyzer::fillHelmTemplateHandlers()
{
  //Order here is important

  _helmTemplateHandlers.emplace_back(
    "ConfigMap",
    [this](const std::vector<std::pair<std::string, YAML::Node>>& configmaps_)
    {
      this->processConfigMaps(configmaps_);
    });
  _helmTemplateHandlers.emplace_back(
    "Secret",
    [this](const std::vector<std::pair<std::string, YAML::Node>>& secrets_)
    {
      this->processSecrets(secrets_);
    });
  _helmTemplateHandlers.emplace_back(
    "Deployment",
    [this](const std::vector<std::pair<std::string, YAML::Node>>& deployments_)
    {
      this->processWorkloads(deployments_);
    });
  _helmTemplateHandlers.emplace_back(
    "StatefulSet",
    [this](const std::vector<std::pair<std::string, YAML::Node>>& statefulsets_)
    {
      this->processWorkloads(statefulsets_);
    });
  _helmTemplateHandlers.emplace_back(
    "DaemonSet",
    [this](const std::vector<std::pair<std::string, YAML::Node>>& daemonsets_)
    {
      this->processWorkloads(daemonsets_);
    });
  _helmTemplateHandlers.emplace_back(
    "Pod",
    [this](const std::vector<std::pair<std::string, YAML::Node>>& pods_)
    {
      this->processWorkloads(pods_);
    });
  _helmTemplateHandlers.emplace_back(
    "Service",
    [this](const std::vector<std::pair<std::string, YAML::Node>>& services_)
    {
      this->processServices(services_);
    });

  _helmTemplateHandlers.emplace_back(
    "KafkaTopic",
    [this](const std::vector<std::pair<std::string, YAML::Node>>& kafkatopics_)
    {
      this->processKafkaTopics(kafkatopics_);
    });
}

void TemplateAnalyzer::fillWorkloadHandlers()
{
  _workloadHandlers["Deployment"] =
    [this](std::unique_ptr<model::Workload>& workload_, const YAML::Node& node_)
    {
      this->processDeployment(workload_, node_);
    };

  _workloadHandlers["StatefulSet"] =
    [this](std::unique_ptr<model::Workload>& workload_, const YAML::Node& node_)
    {
      this->processStatefulSet(workload_, node_);
    };

  _workloadHandlers["DaemonSet"] =
    [this](std::unique_ptr<model::Workload>& workload_, const YAML::Node& node_)
    {
      this->processDaemonSet(workload_, node_);
    };
}

TemplateAnalyzer::~TemplateAnalyzer()
{
  (util::OdbTransaction(_ctx.db))([this]{
    for (model::HelmTemplate& helmTemplate : _newTemplates)
    {
      LOG(info) << helmTemplate.name << " " << helmTemplate.kind
      << " is processed and saved as Helm Template file";
      //_ctx.db->persist(helmTemplate);
    }

    for (model::MSResource& msResource : _msResources)
      _ctx.db->persist(msResource);

//    for (model::Kafkatopic& topic : _kafkaTopicCache)
//      _ctx.db->persist(topic);

//    for (model::Service& service: _serviceCache)
//      _ctx.db->persist(service);

    util::persistAll(_newEdges, _ctx.db);
    util::persistAll(_helmTemplateEdges, _ctx.db);
  });
}

void TemplateAnalyzer::init()
{
  (util::OdbTransaction(_ctx.db))([this]
  {
    std::for_each(
      _helmTemplateHandlers.begin(),
      _helmTemplateHandlers.end(),
      [&, this](const std::pair<
                          std::string,
                          std::function<void(std::vector<std::pair<std::string, YAML::Node>>)>
                        >& templateHandler)
      {
        auto templateIt = _templateCache.find(templateHandler.first);
        if (templateIt != _templateCache.end())
          templateHandler.second(templateIt->second);
        else
          LOG(info) << "No " << templateHandler.first << " templates found in provided Chart(s)!";
      });
//    // Process Configuration resources, like ConfigMap and Secret
//    auto configmapIt = _templateCache.find("ConfigMap");
//    if (configmapIt == _templateCache.end())
//      LOG(info) << "No ConfigMap templates found in provided Chart(s)!";
//    else
//      processConfigMaps(configmapIt->second);
//
//    auto secretIt = _templateCache.find("Secret");
//    if (secretIt == _templateCache.end())
//      LOG(info) << "No Secret templates found in provided Chart(s)!";
//    else
//      processSecrets(secretIt->second);
//
//    // Actual microservice-s stored in Deployments, StatefulSets, DaemonSets, Pods
//    auto deploymentIt = _templateCache.find("Deployment");
//    if (deploymentIt == _templateCache.end())
//      LOG(info) << "No Deployment templates found in provided Chart(s)!";
//    else
//      processWorkloads(deploymentIt->second);

//    auto daemonsetIt = _templateCache.find("DaemonSet");
//    if (daemonsetIt == _templateCache.end())
//      LOG(debug) << "No DaemonSet templates found in provided Chart(s)!";
//    else
//      processWorkloads(daemonsetIt->first, daemonsetIt->second);
//
//    auto statefulSetIt = _templateCache.find("StatefulSet");
//    if (statefulSetIt == _templateCache.end())
//      LOG(debug) << "No StatefulSet templates found in provided Chart(s)!";
//    else
//      processWorkloads(statefulSetIt->first, statefulSetIt->second);
//
//    auto podIt = _templateCache.find("Pod");
//    if (podIt == _templateCache.end())
//      LOG(debug) << "No Pod templates found in provided Chart(s)!";
//    else
//      processWorkloads(podIt->first, podIt->second);

//    auto serviceIt = _templateCache.find("Service");
//    if (serviceIt == _templateCache.end())
//      LOG(info) << "No Service templates found in provided Chart(s)!";
//    else
//      processServices(serviceIt->second);
//
//    auto kafkaTopicIt = _templateCache.find("KafkaTopic");
//    if (kafkaTopicIt == _templateCache.end())
//      LOG(debug) << "No KafkaTopic templates found in provided Chart(s)!";
//    else
//      processKafkaTopics(kafkaTopicIt->second);

//    std::for_each(_fileAstCache.begin(), _fileAstCache.end(),
//      [&, this](std::pair<std::string, std::vector<YAML::Node>> pair)
//      {
//        std::for_each(pair.second.begin(), pair.second.end(),
//          [&, this](YAML::Node& node)
//          {
//            auto currentService = std::find_if(_microserviceCache.begin(),
//              _microserviceCache.end(),
//              [&](model::Microservice& service)
//              {
//                return pair.first.find(service.name) != std::string::npos;
//              });
//
//            if (currentService != _microserviceCache.end())
//              visitKeyValuePairs(pair.first, node, *currentService);
//          });
//      });
//  });

    // Itt valami olyasmi végzés volt, hogy bekerülhettek uj microservice-k amiket lehet elemezni,
    // ha lesz ilyen a fenti ciklusba akkor ez hasznos lehet még

//    for (const model::Microservice& service : _ctx.db->query<model::Microservice>(
//      odb::query<model::Microservice>::type == model::Microservice::ServiceType::PRODUCT))
//    {
//      _microserviceCache.push_back(service);
//    }
//
//    std::for_each(_fileAstCache.begin(), _fileAstCache.end(),
//      [&, this](std::pair<std::string, std::vector<YAML::Node>> pair)
//      {
//        std::for_each(pair.second.begin(), pair.second.end(),
//          [&, this](YAML::Node& node)
//          {
//            auto currentService = std::find_if(_microserviceCache.begin(),
//              _microserviceCache.end(),
//              [&](model::Microservice& service)
//              {
//                return service.type == model::Microservice::ServiceType::PRODUCT &&
//                  pair.first.find("/charts/") == std::string::npos;
//              });
//
//            if (currentService != _microserviceCache.end())
//              visitKeyValuePairs(pair.first, node, *currentService);
//          });
//      });
  });
}

void TemplateAnalyzer::processTemplateCommonProperties(
  model::HelmTemplate& helmTemplate_,
  const std::pair<std::string, YAML::Node>& currentFile_)
{
  (util::OdbTransaction{_ctx.db})([&,this]
  {
    auto filePtr = _ctx.db->query_one<model::File>(odb::query<model::File>::path == currentFile_.first);

    helmTemplate_.name = YAML::Dump(currentFile_.second["metadata"]["name"]);
    helmTemplate_.file = filePtr->id;
    helmTemplate_.kind = YAML::Dump(currentFile_.second["kind"]);
    helmTemplate_.templateType = _dependencyPairs.at(helmTemplate_.kind);
    helmTemplate_.depends = findParentChart(currentFile_.first).chartId;

    helmTemplate_.id = model::createIdentifier(helmTemplate_);
  });
}

void TemplateAnalyzer::processConfigMaps(const std::vector<std::pair<std::string, YAML::Node>>& configmaps_)
{
  (util::OdbTransaction(_ctx.db))([&,this]
  {
    std::for_each(configmaps_.begin(), configmaps_.end(),
      [&, this](const std::pair<std::string, YAML::Node>& pair)
      {
        model::ConfigMap configmap;

        processTemplateCommonProperties(configmap, pair);
        addHelmTemplate(configmap);
      });
  });
}

void TemplateAnalyzer::processSecrets(const std::vector<std::pair<std::string, YAML::Node>>& secrets_)
{
  (util::OdbTransaction(_ctx.db))([&,this]
  {
    std::for_each(secrets_.begin(), secrets_.end(),
      [&, this](const std::pair<std::string, YAML::Node>& pair)
      {
        model::Secret secret;

        processTemplateCommonProperties(secret,pair);
        addHelmTemplate(secret);
      });
  });
}

void TemplateAnalyzer::processWorkloads(
  const std::vector<std::pair<std::string, YAML::Node>>& workloads_)
{
  (util::OdbTransaction(_ctx.db))([&,this]
  {
    std::for_each(workloads_.begin(), workloads_.end(),
      [&, this](const std::pair<std::string, YAML::Node>& pair)
      {
        auto filePtr = _ctx.db->query_one<model::File>(odb::query<model::File>::path == pair.first);
        auto workload = std::make_unique<model::Workload>();

        processTemplateCommonProperties(*workload, pair);

        workload->labels = "";
        if(pair.second["spec"]["template"]["metadata"]["labels"])
        {
          auto labelNode = pair.second["spec"]["template"]["metadata"]["labels"];
          for(auto it = labelNode.begin(); it != labelNode.end(); ++it)
          {
            workload->labels.append(it->first.as<std::string>()).
                              append(":").
                              append(it->second.as<std::string>()).
                              append(";");
          }
          workload->labels.pop_back();
        }

        workload->images = "";
        workload->ports="";
        if(pair.second["spec"]["template"]["spec"]["containers"] &&
           pair.second["spec"]["template"]["spec"]["containers"].IsSequence() &&
           pair.second["spec"]["template"]["spec"]["containers"].size() > 0)
        {
          auto containersNode = pair.second["spec"]["template"]["spec"]["containers"];
          processResources(containersNode, *workload);
          std::vector<YAML::Node> imageKeys;
          findKeys("image", imageKeys, containersNode);
          std::for_each(
            imageKeys.begin(),
            imageKeys.end(),
            [&](const YAML::Node& imageKey)
            {
              workload->images.append(YAML::Dump(imageKey))
                             .append(";");
            });
          workload->images.pop_back();

          std::vector<YAML::Node> portKeys;
          findKeys("ports", portKeys, containersNode);
          std::for_each(
            portKeys.begin(),
            portKeys.end(),
            [&](const YAML::Node& portsKey)
            {
              for(auto portsIt = portsKey.begin(); portsIt != portsKey.end(); ++portsIt)
              {
                for(auto it = portsIt->begin(); it != portsIt->end(); ++it)
                {
                  workload->ports.append(it->first.as<std::string>())
                    .append(":")
                    .append(it->second.as<std::string>())
                    .append(",");
                }
                workload->ports.pop_back();
                workload->ports.append(";");
              }
              workload->ports.pop_back();
            });
        }

        model::Chart dependsOnChart = findParentChart(pair.first);
        workload->depends = dependsOnChart.chartId;

        // save as microservice as well
        std::string microserviceName = workload->name;
        auto microserviceIt = std::find_if(
          _microserviceCache.begin(),
          _microserviceCache.end(),
          [&](const model::Microservice& item)
          {
            return item.name == microserviceName;
          });

        if (microserviceIt == _microserviceCache.end())
        {
          model::Microservice microservice;
          microservice.name = microserviceName;
          microservice.version = YAML::Dump(pair.second["metadata"]["labels"]["app.kubernetes.io\/version"]);
          microservice.file = filePtr->id;

          microservice.microserviceId = model::createIdentifier(microservice);

          _ctx.db->persist(microservice);
          workload->microservice = microservice.microserviceId;

          dependsOnChart.microservice = microservice.microserviceId;
          _ctx.db->update(dependsOnChart);
        }

        _workloadHandlers.find(workload->kind)->second(workload, pair.second);
        addHelmTemplate(*workload);

        auto volumesNode = findKey("volumes", pair.second);
        if (volumesNode.IsDefined())
        {
          for(auto volumeIt = volumesNode.begin(); volumeIt != volumesNode.end(); ++volumeIt)
          {
            if ((*volumeIt)["configMap"] && (*volumeIt)["configMap"].IsDefined() &&
              (*volumeIt)["configMap"]["name"])
            {
              std::string dependentConfigmapName = YAML::Dump((*volumeIt)["configMap"]["name"]);
              auto configmapHelmTemplateIt = std::find_if(
                _helmTemplateCache.begin(),
                _helmTemplateCache.end(),
                [&](model::HelmTemplate& helm)
                {
                  return helm.name == dependentConfigmapName;
                }
              );
              if (configmapHelmTemplateIt != _helmTemplateCache.end())
              {
                addEdge(workload->id, configmapHelmTemplateIt->id);
              }
              else
              {
                LOG(warning) << "Processing "
                             << dependentConfigmapName
                             << " as a ConfigMap dependency of "
                             << workload->name
                             << " "
                             << workload->kind
                             << ", but never processed as an actual ConfigMap. "
                             << "Before installing the Chart make sure this ConfigMap is present or consider using it as an optional mount!";
              }
            }
            else if ((*volumeIt)["secret"] &&
              (*volumeIt)["secret"].IsDefined() &&
              (*volumeIt)["secret"]["secretName"])
            {
              std::string dependentSecretName = YAML::Dump((*volumeIt)["secret"]["secretName"]);
              auto secretHelmTemplateIt = std::find_if(
                _helmTemplateCache.begin(),
                _helmTemplateCache.end(),
                [&](model::HelmTemplate& helm)
                {
                  return helm.name == dependentSecretName;
                }
              );
              if (secretHelmTemplateIt != _helmTemplateCache.end())
              {
                addEdge(workload->id, secretHelmTemplateIt->id);
              }
              else
              {
                LOG(warning) << "Processing "
                             << dependentSecretName
                             << " as a Secret dependency of "
                             << workload->name
                             << " "
                             << workload->kind
                             << ", but never processed as an actual Secret. "
                             << "Before installing the Chart make sure this Secret is present or consider using it as an optional mount!";
              }
            }
          }
        }
      });
  });
}

void TemplateAnalyzer::processDeployment(
  std::unique_ptr<model::Workload>& workload_,
  const YAML::Node& node_)
{
  auto deployment = std::make_unique<model::Deployment>();
  deployment->id = workload_->id;
  deployment->name = workload_->name;
  deployment->kind = workload_->kind;
  deployment->file = workload_->file;
  deployment->ports = workload_->ports;
  deployment->templateType = workload_->templateType;
  deployment->labels = workload_->labels;
  deployment->images = workload_->images;
  deployment->microservice = workload_->microservice;
  deployment->depends = workload_->depends;

  deployment->replicas = 1;
  if (node_["spec"]["replicas"] && node_["spec"]["replicas"].IsScalar())
    deployment->replicas = node_["spec"]["replicas"].as<int32_t>();

  workload_ = std::move(deployment);
}

void TemplateAnalyzer::processStatefulSet(
  std::unique_ptr<model::Workload>& workload_,
  const YAML::Node& node_)
{
  auto statefulSet = std::make_unique<model::StatefulSet>();
  statefulSet->id = workload_->id;
  statefulSet->name = workload_->name;
  statefulSet->kind = workload_->kind;
  statefulSet->file = workload_->file;
  statefulSet->ports = workload_->ports;
  statefulSet->templateType = workload_->templateType;
  statefulSet->labels = workload_->labels;
  statefulSet->images = workload_->images;
  statefulSet->microservice = workload_->microservice;
  statefulSet->depends = workload_->depends;

  statefulSet->replicas = 1;
  if (node_["spec"]["replicas"] && node_["spec"]["replicas"].IsScalar())
    statefulSet->replicas = node_["spec"]["replicas"].as<int32_t>();

  processStorageResources(node_, *statefulSet);

  workload_ = std::move(statefulSet);
}

void TemplateAnalyzer::processDaemonSet(
  std::unique_ptr<model::Workload>& workload_,
  const YAML::Node& node_)
{
  auto daemonSet = std::make_unique<model::DaemonSet>();
  daemonSet->id = workload_->id;
  daemonSet->name = workload_->name;
  daemonSet->kind = workload_->kind;
  daemonSet->file = workload_->file;
  daemonSet->templateType = workload_->templateType;
  daemonSet->ports = workload_->ports;
  daemonSet->labels = workload_->labels;
  daemonSet->images = workload_->images;
  daemonSet->microservice = workload_->microservice;
  daemonSet->depends = workload_->depends;

  daemonSet->tolerations = "";
  daemonSet->nodeSelector = "";
  if(node_["spec"] &&
     node_["spec"].IsDefined() &&
     node_["spec"]["template"] &&
     node_["spec"]["template"].IsDefined() &&
     node_["spec"]["template"]["spec"] &&
     node_["spec"]["template"]["spec"].IsDefined())
  {
    if(node_["spec"]["template"]["spec"]["tolerations"] &&
       node_["spec"]["template"]["spec"]["tolerations"].IsDefined())
    {
      YAML::Node tolerationsNode = node_["spec"]["template"]["spec"]["tolerations"];
      for(auto tolerationIt = tolerationsNode.begin(); tolerationIt != tolerationsNode.end(); ++tolerationIt)
      {
        for(auto it = tolerationIt->begin(); it != tolerationIt->end(); ++it)
        {
          daemonSet->tolerations
            .append(it->first.as<std::string>())
            .append(":")
            .append(it->second.as<std::string>())
            .append(",");
        }
        daemonSet->tolerations.pop_back();
        daemonSet->tolerations.append(";");
      }
      daemonSet->tolerations.pop_back();
    }

    if(node_["spec"]["template"]["spec"]["nodeSelector"] &&
       node_["spec"]["template"]["spec"]["nodeSelector"].IsDefined())
    {
      YAML::Node nodeSelectorNode = node_["spec"]["template"]["spec"]["nodeSelector"];
      for(auto nodeSelectorIt = nodeSelectorNode.begin(); nodeSelectorIt != nodeSelectorNode.end(); ++nodeSelectorIt)
      {
        daemonSet->nodeSelector
          .append(nodeSelectorIt->first.as<std::string>())
          .append(":")
          .append(nodeSelectorIt->second.as<std::string>())
          .append(";");
      }
      daemonSet->nodeSelector.pop_back();
    }
  }

  workload_ = std::move(daemonSet);
}

void TemplateAnalyzer::processServices(const std::vector<std::pair<std::string, YAML::Node>>& services_)
{
  util::OdbTransaction(_ctx.db)([&, this]
  {
    std::for_each(services_.begin(), services_.end(),
      [&](const std::pair<std::string, YAML::Node>& pair)
      {
        auto filePtr = _ctx.db->query_one<model::File>(odb::query<model::File>::path == pair.first);
        // service as a K8s resource, not microservice
        model::Service service;

        service.name = YAML::Dump(pair.second["metadata"]["name"]);
        service.kind = "Service";
        service.templateType = model::HelmTemplate::TemplateType::SERVICE;
        service.file = filePtr->id;

        service.id = model::createIdentifier(service);

        service.type = "ClusterIP"; // default if not provided
        if(pair.second["spec"]["type"])
        {
          service.type = YAML::Dump(pair.second["spec"]["type"]);
        }

        service.ipFamilyPolicy = "SingleStack"; // default if not provided
        if(pair.second["spec"]["ipFamilyPolicy"])
        {
          service.ipFamilyPolicy = YAML::Dump(pair.second["spec"]["ipFamilyPolicy"]);
        }

        service.ports = "";
        if(pair.second["spec"]["ports"] &&
           pair.second["spec"]["ports"].IsSequence())
        {
          auto portsNode = pair.second["spec"]["ports"];
          for(auto portsIt = portsNode.begin(); portsIt != portsNode.end(); ++portsIt)
          {
            for(auto it = portsIt->begin(); it != portsIt->end(); ++it)
            {
              service.ports.append(it->first.as<std::string>())
                .append(":")
                .append(it->second.as<std::string>())
                .append(",");
            }
            service.ports.pop_back();
            service.ports.append(";");
          }
          service.ports.pop_back();
        }

        if (pair.second["spec"]["selector"] &&
            pair.second["spec"]["selector"].IsMap())
        {
          std::vector<std::string> selectors;
          auto selectorIt = pair.second["spec"]["selector"];
          for(auto it = selectorIt.begin(); it != selectorIt.end(); ++it)
          {
            std::string selector;
            selector.append(it->first.as<std::string>())
                    .append(":")
                    .append(it->second.as<std::string>());
            selectors.push_back(selector);
            service.selector.append(selector)
                            .append(";");
          }
          service.selector.pop_back();

          for(auto& deploymentIt : _ctx.db->query<model::Deployment>())
          {
            if(allSelectorsFoundInLabels(deploymentIt.labels, selectors))
            {
              addEdge(service.id, deploymentIt.id);
            }
          }
          //TODO: more here...
        }

        model::Chart dependsOnChart = findParentChart(pair.first);
        service.depends = dependsOnChart.chartId;

        addHelmTemplate(service);
      });
  });
}

bool TemplateAnalyzer::allSelectorsFoundInLabels(
  const std::string& labels,
  const std::vector<std::string>& selectors)
{
  return std::all_of(selectors.begin(), selectors.end(),
    [&labels](const std::string& selector) {
      return labels.find(selector) != std::string::npos;
    });
}

bool TemplateAnalyzer::isValidTemplate(
  const YAML::Node& rootNode)
{
  return rootNode["metadata"] &&
         rootNode["metadata"].IsMap() &&
         rootNode["kind"] &&
         rootNode["metadata"]["name"];
}

bool TemplateAnalyzer::visitKeyValuePairs(
  std::string path_,
  YAML::Node& currentNode_,
  model::Microservice& service_)
{
//  typedef model::HelmTemplate::TemplateType DependencyType;
//
//  if (!currentNode_["metadata"] ||
//      !currentNode_["metadata"].IsMap() ||
//      !currentNode_["kind"] ||
//      !currentNode_["metadata"]["name"])
//    return false;
//
//  auto typeIter = _dependencyPairs.find(YAML::Dump(currentNode_["kind"]));
//  auto type = typeIter->second;
//
//  if (typeIter == _dependencyPairs.end())
//  {
//    if (YAML::Dump(currentNode_["kind"]).find("Certificate") != std::string::npos ||
//        YAML::Dump(currentNode_["kind"]).find("InternalUserCA") != std::string::npos)
//    {
//      type = DependencyType::CERTIFICATE;
//    }
//    else
//    {
//      auto volumesNode = findKey("volumes", currentNode_);
//      if (volumesNode.IsDefined())
//        type = DependencyType::MOUNT;
//      else
//        return false;
//    }
//  }
//
//  switch (type)
//  {
//    case DependencyType::SERVICE:
//      processServiceDeps(path_, currentNode_, service_);
//      break;
//    case DependencyType::MOUNT:
//      processMountDeps(path_, currentNode_, service_);
//      break;
//    case DependencyType::CERTIFICATE:
//      processCertificateDeps(path_, currentNode_, service_);
//      break;
//    case DependencyType::KAFKATOPIC:
//      processKafkaTopicDeps(path_, currentNode_, service_);
//    case DependencyType::RESOURCE:
//    case DependencyType::OTHER:
//      break;
//  }
//
//  processResources(path_, currentNode_, service_);
//  processStorageResources(path_, currentNode_, service_);
//
//  return true;
}

void TemplateAnalyzer::processServiceDeps(
  const std::string& path_,
  YAML::Node& currentFile_,
  model::Microservice& service_)
{
  /* --- Process Service templates --- */

  // Find MS in database.
//  auto serviceIter = std::find_if(_microserviceCache.begin(), _microserviceCache.end(),
//    [&](const model::Microservice& service)
//    {
//      //LOG(info) << service.name << ", " << YAML::Dump(currentFile_["metadata"]["name"]);;
//      return service.name == YAML::Dump(currentFile_["metadata"]["name"]);
//    });
  //LOG(warning) << "service deps: " << serviceIter->name;

  // Persist template data to db.
  model::HelmTemplate helmTemplate;// = std::make_shared<model::HelmTemplate>();
  helmTemplate.templateType = model::HelmTemplate::TemplateType::SERVICE;

  auto filePtr = _ctx.db->query_one<model::File>(odb::query<model::File>::path == path_);
  helmTemplate.file = filePtr->id;
  helmTemplate.kind = YAML::Dump(currentFile_["kind"]);
  helmTemplate.name = YAML::Dump(currentFile_["metadata"]["name"]);
  //helmTemplate.depends = service_.microserviceId;
  helmTemplate.id = createIdentifier(helmTemplate);

  // If the MS is not present in the db,
  // it is an external / central MS,
  // and should be added to the db.
  //LOG(info) << serviceIter->microserviceId << ", " << serviceIter->name;
//  if (serviceIter == _microserviceCache.end())
//  {
//    model::Microservice externalService;
//    externalService.name = YAML::Dump(currentFile_["metadata"]["name"]);
//    externalService.type = model::Microservice::ServiceType::CENTRAL;
//    externalService.file = filePtr->id;
//    externalService.microserviceId = createIdentifier(externalService);
//    externalService.version = YAML::Dump(currentFile_["metadata"]["labels"]["app.kubernetes.io\/version"]);
//    _microserviceCache.push_back(externalService);
//    _ctx.db->persist(externalService);
//
//    helmTemplate.depends = externalService.microserviceId;
//  }
//  else
//  {
//    helmTemplate.depends = serviceIter->microserviceId;
//  }

  addHelmTemplate(helmTemplate);
  //addEdge(service_.microserviceId, helmTemplate.depends, helmTemplate.id, helmTemplate.kind);

//  auto serviceIter = std::find_if(_serviceCache.begin(), _serviceCache.end(),
//    [&](const model::Service& service)
//    {
//      return service.name == YAML::Dump(currentFile_["metadata"]["name"]);
//    });
//
//  if (serviceIter == _serviceCache.end())
//  {
//    model::Service service;
//    service.name = YAML::Dump(currentFile_["metadata"]["name"]);
//    service.type = YAML::Dump(currentFile_["spec"]["type"]);
//    if (currentFile_["spec"]["ipFamilyPolicy"] && currentFile_["spec"]["ipFamilyPolicy"].IsScalar())
//      service.ipFamilyPolicy = YAML::Dump(currentFile_["spec"]["ipFamilyPolicy"]);
//    else
//      service.ipFamilyPolicy = "SingleStack";
//
//    //service.depends = service_.microserviceId;
//    //service.helmTemplateId = helmTemplate.id;
//    //service.serviceId = model::createIdentifier(service);
//
//    _serviceCache.push_back(service);
//    _ctx.db->persist(service);
//  }
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
        //helmTemplate.templateType = model::HelmTemplate::TemplateType::MOUNT;
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
          //helmTemplate.depends = -1;
          helmTemplate.id = createIdentifier(helmTemplate);
        }
        else
        {
          //helmTemplate.depends = serviceIter->microserviceId;
          helmTemplate.id = createIdentifier(helmTemplate);
          //addEdge(service_.microserviceId, helmTemplate.depends, helmTemplate.id, helmTemplate.kind);
        }

        addHelmTemplate(helmTemplate);
      }
      else if ((*volume)["secret"] && (*volume)["secret"]["secretName"])
      {
        model::HelmTemplate helmTemplate;
        //helmTemplate.templateType = model::HelmTemplate::TemplateType::MOUNT;
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
          //helmTemplate.depends = -1;
          helmTemplate.id = createIdentifier(helmTemplate);
        }
        else
        {
          //helmTemplate.depends = serviceIter->microserviceId;
          helmTemplate.id = createIdentifier(helmTemplate);
          //addEdge(service_.microserviceId, helmTemplate.depends, helmTemplate.id, helmTemplate.kind);
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
  //helmTemplate.templateType = model::HelmTemplate::TemplateType::CERTIFICATE;
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
    //helmTemplate.depends = -1;
    helmTemplate.id = createIdentifier(helmTemplate);
  }
  else
  {
    //helmTemplate.depends = serviceIter->microserviceId;
    helmTemplate.id = createIdentifier(helmTemplate);
    //addEdge(service_.microserviceId, helmTemplate.depends, helmTemplate.id, helmTemplate.kind);
  }

  addHelmTemplate(helmTemplate);

  model::HelmTemplate secretTemplate;
  //secretTemplate.templateType = model::HelmTemplate::TemplateType::CERTIFICATE;
  secretTemplate.kind = "Secret";
  secretTemplate.file = filePtr->id;
  secretTemplate.name = YAML::Dump(keyName);

  if (serviceIter == _microserviceCache.end())
  {
    //secretTemplate.depends = -1;
    secretTemplate.id = createIdentifier(secretTemplate);
  }
  else
  {
    //secretTemplate.depends = serviceIter->microserviceId;
    secretTemplate.id = createIdentifier(secretTemplate);
    //addEdge(service_.microserviceId, secretTemplate.depends, secretTemplate.id, secretTemplate.kind);
  }

  addHelmTemplate(secretTemplate);
}

void TemplateAnalyzer::processKafkaTopics(
  const std::vector<std::pair<std::string, YAML::Node>>& kafkaTopics_)
{
  util::OdbTransaction(_ctx.db)([&,this]
  {
    std::for_each(kafkaTopics_.begin(), kafkaTopics_.end(),
      [&,this](const std::pair<std::string, YAML::Node>& pair)
      {
        model::KafkaTopic kafkaTopic;
        kafkaTopic.templateType = model::HelmTemplate::TemplateType::KAFKATOPIC;
        auto filePtr = _ctx.db->query_one<model::File>(odb::query<model::File>::path == pair.first);
        kafkaTopic.file = filePtr->id;
        kafkaTopic.kind = YAML::Dump(pair.second["kind"]);
        kafkaTopic.name = YAML::Dump(pair.second["metadata"]["name"]);
        kafkaTopic.topicName = YAML::Dump(pair.second["spec"]["topicName"]);
        if (pair.second["spec"]["replicas"] && pair.second["spec"]["replicas"].IsScalar())
          kafkaTopic.replicaCount = pair.second["spec"]["replicas"].as<uint64_t>();
        else
          kafkaTopic.replicaCount = 1;
        if (pair.second["spec"]["partitions"] && pair.second["spec"]["partitions"].IsScalar())
          kafkaTopic.partitionCount = pair.second["spec"]["partitions"].as<uint64_t>();
        else
          kafkaTopic.partitionCount = 1;

        kafkaTopic.id = createIdentifier(kafkaTopic);

        model::Chart dependsOnChart = findParentChart(pair.first);

        kafkaTopic.depends = dependsOnChart.chartId;

        addHelmTemplate(kafkaTopic);
      });
  });
}

model::Chart TemplateAnalyzer::findParentChart(const std::string& templatePath_)
{
  size_t max_index = std::string::npos; // Kezdetben nincs találat
  model::Chart parentChart;

  for (const auto& chart : _chartCache)
  {
    size_t pos;
    if(chart.alias.empty())
      pos = templatePath_.rfind(chart.name);
    else
      pos = templatePath_.rfind(chart.alias);
    if (pos != std::string::npos && (max_index == std::string::npos || pos > max_index))
    {
      max_index = pos;
      parentChart = chart;
    }
  }

  return parentChart;
}

void TemplateAnalyzer::processResources(
  YAML::Node& node_,
  model::Workload& workload_)
{
  std::vector<YAML::Node> resourceKeys;
  findKeys("resources", resourceKeys, node_);

  std::for_each(
    resourceKeys.begin(),
    resourceKeys.end(),
    [&,this](const YAML::Node& resourceKey)
    {
      if(resourceKey["requests"] &&
         resourceKey["requests"].IsDefined())
      {
        for(auto it = resourceKey["requests"].begin(); it != resourceKey["requests"].end(); ++it)
        {
          auto resourcePairIt = _msResourcePairs.find(YAML::Dump(it->first));
          if(resourcePairIt == _msResourcePairs.end())
            return;

          model::MSResource resource;
          resource.type = resourcePairIt->second;
          auto convertedAmount = convertUnit(
            YAML::Dump(it->second),
            resourcePairIt->second);
          resource.requestedAmount = convertedAmount.first;
          resource.unit = convertedAmount.second;

          if(resourceKey["limits"] &&
             resourceKey["limits"].IsDefined() &&
             resourceKey["limits"][it->first] &&
            resourceKey["limits"][it->first].IsDefined())
          {
            convertedAmount = convertUnit(
              YAML::Dump(resourceKey["limits"][it->first]),
              resourcePairIt->second);
            resource.limitAmount = convertedAmount.first;
          } else
          {
            resource.limitAmount = resource.requestedAmount;
          }

          resource.helmTemplateId = workload_.id;
          _msResources.push_back(resource);
        }
      }
    });
}

void TemplateAnalyzer::processStorageResources(
  const YAML::Node& node_,
  model::Workload& workload_)
{
  /* --- Collect storage resources --- */
  YAML::Node volumeClaimsKey = findKey("volumeClaimTemplates", node_);
  if (!volumeClaimsKey.IsDefined())
    return;

  processResources(volumeClaimsKey, workload_);
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
        return std::make_pair(std::stof(amount_) / 1024, "Gi");
      }

      int g = amount_.find('G');
      if (g != std::string::npos)
      {
        amount_.erase(g, 2);
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
  auto it = std::find_if(_helmTemplateCache.begin(), _helmTemplateCache.end(),
    [&](auto& helm)
    {
        return helm.id == helmTemplate_.id;
    });

  if (it == _helmTemplateCache.end())
  {
    _helmTemplateCache.push_back(helmTemplate_);
    _ctx.db->persist(helmTemplate_);
  }
}

void TemplateAnalyzer::addEdge(
  model::HelmTemplateId from_,
  model::HelmTemplateId to_
)
{
  static std::mutex m;
  std::lock_guard<std::mutex> guard(m);

  model::HelmTemplateDependencyEdgePtr edge = std::make_shared<model::HelmTemplateDependencyEdge>();

  edge->from = std::make_shared<model::HelmTemplate>();
  edge->from->id = from_;

  edge->to = std::make_shared<model::HelmTemplate>();
  edge->to->id = to_;

  edge->id = model::createIdentifier(edge);

  if(_helmTemplateEdgeCache.insert(edge->id).second)
  {
    _helmTemplateEdges.push_back(edge);
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

  model::DependencyEdgePtr edge = std::make_shared<model::DependencyEdge>();

  edge->from = std::make_shared<model::Microservice>();
  edge->from->microserviceId = from_;
  edge->to = std::make_shared<model::Microservice>();
  edge->to->microserviceId = to_;

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

void TemplateAnalyzer::fillDependencyPairsMap()
{
  _dependencyPairs.insert({"Service", model::HelmTemplate::TemplateType::SERVICE});
  _dependencyPairs.insert({"ConfigMap", model::HelmTemplate::TemplateType::CONFIGMAP});
  _dependencyPairs.insert({"Secret", model::HelmTemplate::TemplateType::SECRET});
  _dependencyPairs.insert({"Deployment", model::HelmTemplate::TemplateType::DEPLOYMENT});
  _dependencyPairs.insert({"StatefulSet", model::HelmTemplate::TemplateType::STATEFULSET});
  _dependencyPairs.insert({"DaemonSet", model::HelmTemplate::TemplateType::DAEMONSET});
  _dependencyPairs.insert({"Pod", model::HelmTemplate::TemplateType::POD});
  _dependencyPairs.insert({"KafkaTopic", model::HelmTemplate::TemplateType::KAFKATOPIC});
}

void TemplateAnalyzer::fillResourceTypePairsMap()
{
  _msResourcePairs.insert({"cpu", model::MSResource::ResourceType::CPU});
  _msResourcePairs.insert({"memory", model::MSResource::ResourceType::MEMORY});
  _msResourcePairs.insert({"storage", model::MSResource::ResourceType::STORAGE});
  _msResourcePairs.insert({"ephemeral-storage", model::MSResource::ResourceType::MEMORY});
}

}
}