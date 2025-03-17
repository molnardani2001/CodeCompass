#ifndef CC_PARSER_YAML_PARSER_H
#define CC_PARSER_YAML_PARSER_H

#include <atomic>

#include <parser/abstractparser.h>
#include <parser/parsercontext.h>

#include <util/parserutil.h>

#include "yaml-cpp/yaml.h"
#include "model/chart.h"

namespace cc
{
namespace parser
{
class YamlParser : public AbstractParser
{
public:
  YamlParser(ParserContext& ctx_);
  ~YamlParser();
  virtual bool cleanupDatabase() override;
  virtual bool parse() override;

private:
  /**
   * This method classifies a YAML file according to the purpose
   * it serves, e.g. Helm chart or other type of configuration file.
   * The classification is done based on naming conventions.
   */
  void processFileType(
    model::FilePtr& file_,
    YAML::Node& loadedFile);

  void processIntegrationChart(
    model::FilePtr& file_,
    YAML::Node& loadedFile,
    model::Chart& chart);

  /**
   * The purpose of this function is to implement the functionality of helm values management.
   * When there is an integration chart, then its values.yaml can (and usually does) overwriting/adding lobal
   * values for children charts. There are 2 specific rules:
   * - an integration chart's `global` values is going to be passed down to the child chart values
   *   (even if child chart has `global` in its own values, parent's will overwrite it/merge with them,
   *   keeping higher precedence on parent's values)
   * - if an integration chart's values.yaml contains the name of its subchart on the first-level in YAML definition,
   *   then the values under that entry will overwrite/be added to the child chart's values.yaml
   *
   * @param path_ Full path to child chart values.yaml
   * @param values Child chart's values.yaml as YAML::Node
   */
  void mergeIntegrationValues(
    const boost::filesystem::path& path_,
    YAML::Node& values
    );

  /**
   * The first-level keys in a YAML files usually have great significance,
   * so they should be collected in a separate collection.
   * @param file_
   * @param loadedFile
   */
  void processRootKeys(
    model::FilePtr& file_,
    YAML::Node& loadedFile);

  bool collectAstNodes(model::FilePtr file_);

  /**
   * This method is used to decide the type of root keys and values.
   */
  void chooseCoreNodeType(
    YAML::Node& node_,
    model::FilePtr file_,
    model::YamlAstNode::SymbolType symbolType_);

  /**
   * These methods handle the different key and value types
   * in YAML files.
   */
  void processAtomicNode(
    YAML::Node& node_,
    model::FilePtr file_,
    model::YamlAstNode::SymbolType symbolType_,
    model::YamlAstNode::AstType astType_);
  void processMap(
    YAML::Node& node_,
    model::FilePtr file_,
    model::YamlAstNode::SymbolType symbolType_);
  void processSequence(
    YAML::Node& node_,
    model::FilePtr file_,
    model::YamlAstNode::SymbolType symbolType_);

  model::Range getNodeLocation(YAML::Node& node_);

  static YAML::Node getChildNode(
    const YAML::Node& node_,
    const std::deque<std::string>& keys_);

  static void mergeNodes(
    const YAML::Node& source_,
    YAML::Node& target_);

  /**
   * A method to recursively traverse the input directory and
   * find YAML files.
   */
  util::DirIterCallback getParserCallback();

  bool accept(const std::string& path_) const;

  std::unordered_set<model::FileId> _fileIdCache;
  std::map<std::string, std::vector<YAML::Node>> _fileAstCache;
  std::map<std::string, YAML::Node> _valuesAstCache;
  std::map<std::string, YAML::Node> _integrationValuesCache;

  std::map<std::string, YAML::Node> _globalValuesCache;
  std::map<std::string, std::vector<std::pair<std::string, YAML::Node>>> _templateCache;
  std::unique_ptr<util::JobQueueThreadPool<std::string>> _pool;
  std::atomic<int> _visitedFileCount;
  std::vector<model::YamlAstNodePtr> _astNodes;
  std::vector<model::YamlFilePtr> _yamlFiles;
  std::vector<model::YamlContentPtr> _rootPairs;
  std::vector<model::BuildLog> _buildLogs;
  std::vector<std::string> _processedMS;
  std::vector<model::Chart> _chartCache;
  std::vector<model::ChartDependencyEdgePtr> _chartEdges;

  std::mutex _mutex;
  bool _areDependenciesListed;
};

} // namespace parser
} // namespace cc

#endif // CC_PARSER_YAML_PARSER_H
