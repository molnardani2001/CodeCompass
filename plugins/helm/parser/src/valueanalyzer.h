#ifndef CC_PARSER_YAMLRELATIONCOLLECTOR_H
#define CC_PARSER_YAMLRELATIONCOLLECTOR_H

#include "yaml-cpp/yaml.h"

#include "model/file.h"

#include <model/microservice.h>
#include <model/microservice-odb.hxx>
#include <model/dependencyedge.h>
#include <model/dependencyedge-odb.hxx>
#include <model/helmtemplate.h>
#include <model/helmtemplate-odb.hxx>

#include <parser/parsercontext.h>

namespace cc
{
namespace parser
{

class ValueAnalyzer
{
public:
  ValueAnalyzer(
    ParserContext& ctx_,
    std::map<std::string, YAML::Node>& fileAstCache_,
    uint64_t templateIdCounter);

  void init();

  ~ValueAnalyzer();

private:
  YAML::Node findValue(
    std::string value_,
    YAML::Node& currentFile_);

  std::shared_ptr<model::HelmTemplate> findHelmTemplate(
      model::HelmTemplateId helmTemplateId);

  void addEdge(
    const model::MicroserviceId& from_,
    const model::MicroserviceId& to_,
    const model::HelmTemplateId& connect_,
    std::string type_);

  bool visitKeyValuePairs(
    YAML::Node& currentNode_,
    model::Microservice& service_,
    const model::FilePtr& file_);

  void addHelmTemplate(
    model::HelmTemplate& helmTemplate_);

  static std::unordered_set<model::DependencyEdgeId> _edgeCache;
  std::vector<model::DependencyEdgePtr> _newEdges;
  std::vector<model::HelmTemplate> _newTemplates;
  uint64_t _templateCounter;

  static std::vector<model::Microservice> _microserviceCache;

  static std::vector<model::Service> _serviceCache;
  static std::vector<model::KafkaTopic> _kafkaTopicCache;
  static std::vector<model::Chart> _chartCache;

  static std::mutex _edgeCacheMutex;

  ParserContext& _ctx;
  std::map<std::string, YAML::Node>& _fileAstCache;
};

}
}

#endif // CC_PARSER_YAMLRELATIONCOLLECTOR_H
