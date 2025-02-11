#ifndef CC_PARSER_TEMPLATEANALYZER_H
#define CC_PARSER_TEMPLATEANALYZER_H

#include "yaml-cpp/yaml.h"

#include "model/file.h"

#include <model/microservice.h>
#include <model/microservice-odb.hxx>
#include <model/dependencyedge.h>
#include <model/dependencyedge-odb.hxx>
#include <model/msresource.h>
#include <model/msresource-odb.hxx>
#include <model/helmtemplate.h>
#include <model/helmtemplate-odb.hxx>
#include <model/helmtemplatedependencyedge.h>
#include <model/helmtemplatedependencyedge-odb.hxx>
//#include <model/kafkatopic.h>
//#include <model/kafkatopic-odb.hxx>
//#include <model/service.h>
//#include <model/service-odb.hxx>

#include <parser/parsercontext.h>

namespace cc
{
namespace parser
{

class TemplateAnalyzer
{
public:
  TemplateAnalyzer(
    ParserContext& ctx_,
    std::map<std::string, std::vector<YAML::Node>>& fileAstCache_,
    std::map<std::string, std::vector<std::pair<std::string, YAML::Node>>>& templateCache_);

  ~TemplateAnalyzer();

  void init();
  uint64_t getTemplateCounter() { return templateCounter; }

private:
  bool visitKeyValuePairs(
    std::string path_,
    YAML::Node& currentFile_,
    model::Microservice& service_);

  /**
   *
   * @param path_ The currently processed file path.
   * @param currentFile_ The currently processed file as a YAML node.
   * @param service_ The microservice in which the file is defined.
   */
  void processServiceDeps(
    const std::string& path_,
    YAML::Node& currentFile_,
    model::Microservice& service_);

  /**
   *
   * @param path_ The currently processed file path.
   * @param currentFile_ The currently processed file as a YAML node.
   * @param service_ The microservice in which the file is defined.
   */
  void processMountDeps(
    const std::string& path_,
    YAML::Node& currentFile_,
    model::Microservice& service_);

  /**
   *
   * @param path_ The currently processed file path.
   * @param currentFile_ The currently processed file as a YAML node.
   * @param service_ The microservice in which the file is defined.
   */
  void processCertificateDeps(
    const std::string& path_,
    YAML::Node& currentFile_,
    model::Microservice& service_);

  void processResources(
    YAML::Node& node_,
    model::Workload& workload_);

  void processStorageResources(
    const YAML::Node& node_,
    model::Workload& workload_);

  bool isValidTemplate(const YAML::Node& rootNode);

  static bool allSelectorsFoundInLabels(
    const std::string& labels,
    const std::vector<std::string>& selectors);

  void processTemplateCommonProperties(
    model::HelmTemplate& helmTemplate_,
    const std::pair<std::string, YAML::Node>& currentFile_);

  void processWorkloads(
    const std::vector<std::pair<std::string, YAML::Node>>& workloads_);

  void processConfigMaps(
    const std::vector<std::pair<std::string, YAML::Node>>& configmaps_);

  void processSecrets(
    const std::vector<std::pair<std::string, YAML::Node>>& secrets_);

  void processKafkaTopics(
    const std::vector<std::pair<std::string, YAML::Node>>& kafkaTopics_);

  void processServices(
    const std::vector<std::pair<std::string, YAML::Node>>& services_);

  void processDeployment(
    std::unique_ptr<model::Workload>& workload_, const YAML::Node& node_);

  void processDaemonSet(
    std::unique_ptr<model::Workload>& workload_, const YAML::Node& node_);

  void processStatefulSet(
    std::unique_ptr<model::Workload>& workload_, const YAML::Node& node_);

  void addHelmTemplate(model::HelmTemplate& helmTemplate_);

  void addEdge(
    model::HelmTemplateId from_,
    model::HelmTemplateId to_
  );

  void addEdge(
    const model::MicroserviceId& from_,
    const model::MicroserviceId& to_,
    const model::HelmTemplateId& connect_,
    std::string type_);

  void fillDependencyPairsMap();
  void fillResourceTypePairsMap();

  void fillHelmTemplateHandlers();
  void fillWorkloadHandlers();

  std::pair<float, std::string> convertUnit(
    std::string amount_,
    model::MSResource::ResourceType type_);

  YAML::Node findKey(
    const std::string& key_,
    const YAML::Node& currentFile_);

  std::vector<YAML::Node> findKeys(
    const std::string& key_,
    std::vector<YAML::Node>& nodes_,
    YAML::Node& node_);

  static model::Chart findParentChart(
    const std::string& templatePath_);

  std::map<std::string, model::HelmTemplate::TemplateType> _dependencyPairs;
  std::map<std::string, model::MSResource::ResourceType> _msResourcePairs;

  std::vector<std::pair<
    std::string,
    std::function<void(std::vector<std::pair<std::string, YAML::Node>>)>>> _helmTemplateHandlers;

  std::map<std::string, std::function<void(std::unique_ptr<model::Workload>&, const YAML::Node&)>> _workloadHandlers;

  static std::unordered_set<model::DependencyEdgeId> _edgeCache;
  std::vector<model::DependencyEdgePtr> _newEdges;

  static std::unordered_set<model::HelmTemplateDependencyEdgeId> _helmTemplateEdgeCache;
  std::vector<model::HelmTemplateDependencyEdgePtr> _helmTemplateEdges;

  std::vector<model::HelmTemplate> _newTemplates;
  uint64_t templateCounter;

  static std::vector<model::Microservice> _microserviceCache;
  static std::vector<model::Chart> _chartCache;

  //static std::vector<model::Kafkatopic> _kafkaTopicCache;
  //static std::vector<model::Service> _serviceCache;
  static std::vector<model::HelmTemplate> _helmTemplateCache;

  std::vector<model::MSResource> _msResources;

  static std::mutex _edgeCacheMutex;

  ParserContext& _ctx;
  std::map<std::string, std::vector<YAML::Node>>& _fileAstCache;
  std::map<std::string, std::vector<std::pair<std::string, YAML::Node>>>& _templateCache;
};
}
}


#endif // CC_PARSER_TEMPLATEANALYZER_H
