#ifndef CC_SERVICE_LANGUAGE_CSHARPFILEDIAGRAM_H
#define CC_SERVICE_LANGUAGE_CSHARPFILEDIAGRAM_H

#include <service/csharpservice.h>
#include <projectservice/projectservice.h>
#include <util/graph.h>

namespace cc
{
namespace service
{
namespace language
{

typedef std::map<std::string, std::vector<std::string>> Usages;
typedef std::map<std::string, std::vector<Usages>> DirectoryUsages;

class CsharpFileDiagram
{
public:
  CsharpFileDiagram(
    std::shared_ptr<odb::database> db_,
    std::shared_ptr<std::string> datadir_,
    const cc::webserver::ServerContext& context_);
  
  /**
   * This diagram shows directories (modules) that are users of the
   * queried module.
   */
  void getExternalUsersDiagram(
   util::Graph& graph_,
   const core::FileId& fileId_,
   const DirectoryUsages& dirRevUsages_);

  /**
   * This function creates legend for the External users diagram.
   * @return The generated legend as a string in SVG format.
   */
  std::string getExternalUsersDiagramLegend();

  /**
   * This diagram shows of the aggregation and association 
   * related file "use" and "used" dependencies.
   */
  void getFileUsagesDiagram(
    util::Graph& graph_,
    const core::FileId& fileId_,
    const Usages& useIds_,
    const Usages& revUseIds_);

  /**
   * This function creates legend for the Include dependency diagram.
   * @return The generated legend as a string in SVG format.
   */
  std::string getFileUsagesDiagramLegend();

  /**
   * This diagram shows the directories relationship between the subdirectories
   * of the queried module. This diagram is useful to understand the
   * relationships of the subdirectories (submodules) of a module.
   */
  void getSubsystemDependencyDiagram(
    util::Graph& graph_,
   const core::FileId& fileId_);

  /**
   * This function creates legend for the Subsystem dependency diagram.
   * @return The generated legend as a string in SVG format.
   */
  std::string getSubsystemDependencyDiagramLegend();

private:
  typedef std::vector<std::pair<std::string, std::string>> Decoration;

  /**
   * This function gathers the fileInfo related to the given file IDs
   * and returns a list of nodes using the file infos.
  */
  std::vector<util::Graph::Node> getNodesFromIds(
  util::Graph& graph,
  const std::vector<core::FileId>& fileIds);

  /**
   * This function adds a node which represents a File node. The label of the
   * node is the file name.
   */
  util::Graph::Node addNode(
    util::Graph& graph_,
    const core::FileInfo& fileInfo_);

  /**
   * This function decorates a graph node.
   * @param graph_ A graph object.
   * @param elem_ A graph node
   * @param decoration_ A map which describes the style attributes.
   */
  void decorateNode(
    util::Graph& graph_,
    const util::Graph::Node& node_,
    const Decoration& decoration_) const;

  /**
   * This function decorates a graph edge.
   * @param graph_ A graph object.
   * @param elem_ A graph edge
   * @param decoration_ A map which describes the style attributes.
   */
  void decorateEdge(
    util::Graph& graph_,
    const util::Graph::Edge& edge_,
    const Decoration& decoration_) const;

  /**
   * Returns the last `n` parts of the path with a '...' if the part size is
   * larger than `n`.
   * (E.g.: path = `/a/b/c/d` and n = `3` then the is result = .../b/c/d)
   */
  std::string getLastNParts(const std::string& path_, std::size_t n_);

  /**
   * This function creates graph nodes for sub directories of the actual file
   * node and returns the created graph nodes.
   * @param graph_ A graph object.
   * @param fileNode_ Graph file node object which represents a file object.
   * @return Subdirectory graph nodes.
   */
  std::vector<util::Graph::Node> getSubDirs(
   util::Graph& graph_,
   const util::Graph::Node& fileNode_);

  static const Decoration centerNodeDecoration;
  static const Decoration sourceFileNodeDecoration;
  static const Decoration directoryNodeDecoration;

  static const Decoration usagesEdgeDecoration;
  static const Decoration revUsagesEdgeDecoration;
  static const Decoration subdirEdgeDecoration;

  std::shared_ptr<odb::database> _db;
  util::OdbTransaction _transaction;
  core::ProjectServiceHandler _projectHandler;
};

} // language
} // service
} // cc

#endif // CC_SERVICE_LANGUAGE_FILEDIAGRAM_H
