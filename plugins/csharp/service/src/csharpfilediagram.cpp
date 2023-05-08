#include <service/csharpservice.h>
#include <projectservice/projectservice.h>
#include <util/graph.h>

#include <util/logutil.h>
#include <util/dbutil.h>
#include <util/legendbuilder.h>

#include <model/file.h>
#include <model/file-odb.hxx>

#include "csharpfilediagram.h"


namespace cc
{
namespace service
{
namespace language
{

typedef odb::query<model::File> FileQuery;
typedef odb::result<model::File> FileResult;
typedef std::map<std::string, std::vector<std::string>> Usages;
typedef std::map<std::string, std::vector<Usages>> DirectoryUsages;

CsharpFileDiagram::CsharpFileDiagram(
  std::shared_ptr<odb::database> db_,
  std::shared_ptr<std::string> datadir_,
  const cc::webserver::ServerContext& context_)
    : _db(db_),
      _transaction(db_),
      _projectHandler(db_, datadir_, context_)
{
}

void CsharpFileDiagram::getFileUsagesDiagram(
  util::Graph& graph_,
  const core::FileId& fileId_,
  const Usages& useIds_,
  const Usages& revUseIds_)
{
  core::FileInfo fileInfo;
  _projectHandler.getFileInfo(fileInfo, fileId_);
  util::Graph::Node currentNode = addNode(graph_, fileInfo);
  decorateNode(graph_, currentNode, centerNodeDecoration);

  //create nodes for use cases
  for (const auto& entry : useIds_)
  {
    core::FileId fileId = entry.first;
    core::FileInfo fileInfo;
    _projectHandler.getFileInfo(fileInfo, fileId);
    util::Graph::Node currentNode = addNode(graph_, fileInfo);
    
    for (const auto& value : entry.second)
    {
      core::FileId fileId = value;
      core::FileInfo fileInfo;
      _projectHandler.getFileInfo(fileInfo, fileId);
      util::Graph::Node toNode = addNode(graph_, fileInfo);

      util::Graph::Edge useEdge = graph_.createEdge(currentNode, toNode);
      decorateEdge(graph_, useEdge, usagesEdgeDecoration);

    }
  }

  //create nodes for revuse cases
  for (const auto& entry : revUseIds_)
  {
    core::FileId fileId = entry.first;
    core::FileInfo fileInfo;
    _projectHandler.getFileInfo(fileInfo, fileId);
    util::Graph::Node currentNode = addNode(graph_, fileInfo);
    
    for (const auto& value : entry.second)
    {
      core::FileId fileId = value;
      core::FileInfo fileInfo;
      _projectHandler.getFileInfo(fileInfo, fileId);
      util::Graph::Node toNode = addNode(graph_, fileInfo);

      util::Graph::Edge useEdge = graph_.createEdge(currentNode, toNode);
      decorateEdge(graph_, useEdge, revUsagesEdgeDecoration);

    }
  }

  decorateNode(graph_, currentNode, centerNodeDecoration);
}

std::string CsharpFileDiagram::getFileUsagesDiagramLegend()
{
  util::LegendBuilder builder("File Usages Diagram");

  builder.addNode("center node", centerNodeDecoration);
  builder.addNode("source file", sourceFileNodeDecoration);

  builder.addEdge("uses", usagesEdgeDecoration);
  builder.addEdge("used by files", revUsagesEdgeDecoration);

  return builder.getOutput();
}

void CsharpFileDiagram::getExternalUsersDiagram(
  util::Graph& graph_,
  const core::FileId& fileId_,
  const DirectoryUsages& dirRevUsages_)
{
  core::FileInfo fileInfo;
  _projectHandler.getFileInfo(fileInfo, fileId_);
  util::Graph::Node currentNode = addNode(graph_, fileInfo);
  decorateNode(graph_, currentNode, centerNodeDecoration);
  
  //to get the directory tree
  getSubsystemDependencyDiagram(graph_,fileId_);

  for (const auto& map : dirRevUsages_)
  {
    core::FileId fileId = map.first;
    core::FileInfo fInfo;
    _projectHandler.getFileInfo(fInfo, fileId);
    util::Graph::Node dirNode = addNode(graph_, fInfo);

    for (const auto& innerMap : map.second)
    {
      for (const auto& revUseIds : innerMap)
      {
        for (const auto& revUseId : revUseIds.second)
        {
          core::FileInfo info;

          _projectHandler.getFileInfo(info, revUseId);
          if (info.path.find(fileInfo.path + '/') == std::string::npos)
          {
            core::FileInfo externalDirInfo;
            _projectHandler.getFileInfo(externalDirInfo, info.parent);
            util::Graph::Node externalDirNode = addNode(graph_,externalDirInfo);

            if (!graph_.hasEdge(dirNode,externalDirNode))
            {
              util::Graph::Edge edge = graph_.createEdge(dirNode,externalDirNode);
              decorateEdge(graph_, edge, revUsagesEdgeDecoration);
            }
          }
        }
      }
    }
  }
}

std::string CsharpFileDiagram::getExternalUsersDiagramLegend()
{
  util::LegendBuilder builder("External Users Diagram");

  builder.addNode("center file", centerNodeDecoration);
  builder.addNode("directory", directoryNodeDecoration);

  builder.addEdge("sub directory", subdirEdgeDecoration);
  builder.addEdge("used by", revUsagesEdgeDecoration);

  return builder.getOutput();
}

void CsharpFileDiagram::getSubsystemDependencyDiagram(
  util::Graph& graph_,
  const core::FileId& fileId_)
{
  core::FileInfo fileInfo;
  _projectHandler.getFileInfo(fileInfo, fileId_);
  util::Graph::Node currentNode = addNode(graph_, fileInfo);
  decorateNode(graph_, currentNode, centerNodeDecoration);
  decorateNode(graph_, currentNode, directoryNodeDecoration);

  std::set<util::Graph::Node> subdirs = util::bfsBuild(graph_, currentNode,
    std::bind(&CsharpFileDiagram::getSubDirs, this, std::placeholders::_1,
    std::placeholders::_2), {}, subdirEdgeDecoration);

  subdirs.insert(currentNode);
}

std::string CsharpFileDiagram::getSubsystemDependencyDiagramLegend()
{
  util::LegendBuilder builder("Subsystem Dependency Diagram");

  builder.addNode("center file", centerNodeDecoration);
  builder.addNode("directory", directoryNodeDecoration);

  builder.addEdge("sub directory", subdirEdgeDecoration);

  return builder.getOutput();
}

std::vector<util::Graph::Node> CsharpFileDiagram::getSubDirs(
  util::Graph& graph_,
  const util::Graph::Node& node_)
{
  std::vector<util::Graph::Node> usages;

  _transaction([&, this]{
    FileResult sub = _db->query<model::File>(
      FileQuery::parent == std::stoull(node_) &&
      FileQuery::type == model::File::DIRECTORY_TYPE);

    for (const model::File& subdir : sub)
    {
      core::FileInfo fileInfo;
      _projectHandler.getFileInfo(fileInfo, std::to_string(subdir.id));

      usages.push_back(addNode(graph_, fileInfo));
    }
  });

  return usages;
}

std::vector<util::Graph::Node> CsharpFileDiagram::getNodesFromIds(
  util::Graph& graph,
  const std::vector<core::FileId>& fileIds)
{
  std::vector<util::Graph::Node> usages;

  for (const core::FileId& fileId: fileIds)
  {
    core::FileInfo fileInfo;
    _projectHandler.getFileInfo(fileInfo, fileId);

    usages.push_back(addNode(graph, fileInfo));
  }

  return usages;
}

util::Graph::Node CsharpFileDiagram::addNode(
  util::Graph& graph_,
  const core::FileInfo& fileInfo_)
{
  util::Graph::Node node = graph_.getOrCreateNode(fileInfo_.id);
  graph_.setNodeAttribute(node, "label", getLastNParts(fileInfo_.path, 3));

  if (fileInfo_.type == model::File::DIRECTORY_TYPE)
  {
    decorateNode(graph_, node, directoryNodeDecoration);
  }
  else if (fileInfo_.type == "CS")
  {
    std::string ext = boost::filesystem::extension(fileInfo_.path);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    decorateNode(graph_, node, sourceFileNodeDecoration);
  }

  return node;
}

void CsharpFileDiagram::decorateNode(
  util::Graph& graph_,
  const util::Graph::Node& node_,
  const Decoration& decoration_) const
{
  for (const auto& attr : decoration_)
    graph_.setNodeAttribute(node_, attr.first, attr.second);
}

void CsharpFileDiagram::decorateEdge(
  util::Graph& graph_,
  const util::Graph::Edge& edge_,
  const Decoration& decoration_) const
{
  for (const auto& attr : decoration_)
    graph_.setEdgeAttribute(edge_, attr.first, attr.second);
}

std::string CsharpFileDiagram::getLastNParts(const std::string& path_, std::size_t n_)
{
  if (path_.empty() || n_ == 0)
    return "";

  std::size_t p;
  for (p = path_.rfind('/');
       --n_ > 0 && p - 1 < path_.size();
       p = path_.rfind('/', p - 1));

  return p > 0 && p < path_.size() ? "..." + path_.substr(p) : path_;
}

const CsharpFileDiagram::Decoration CsharpFileDiagram::centerNodeDecoration = {
  {"shape", "ellipse"},
  {"style", "filled"},
  {"fillcolor", "gold"},
  {"fontcolor", "black"}
};

const CsharpFileDiagram::Decoration CsharpFileDiagram::sourceFileNodeDecoration = {
  {"shape", "box"},
  {"style", "filled"},
  {"fillcolor", "#116db6"},
  {"fontcolor", "white"}
};

const CsharpFileDiagram::Decoration CsharpFileDiagram::directoryNodeDecoration = {
  {"shape", "folder"}
};

const CsharpFileDiagram::Decoration CsharpFileDiagram::usagesEdgeDecoration = {
  {"label", "uses"}
};

const CsharpFileDiagram::Decoration CsharpFileDiagram::revUsagesEdgeDecoration = {
  {"label", "used by"}
};

const CsharpFileDiagram::Decoration CsharpFileDiagram::subdirEdgeDecoration = {
  {"label", "subdir"},
  {"color", "green"}
};

} // language
} // service
} // cc