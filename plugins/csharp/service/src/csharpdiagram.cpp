#include <service/csharpservice.h>
#include <projectservice/projectservice.h>
#include <util/graph.h>

#include <util/logutil.h>
#include <util/dbutil.h>
#include <util/legendbuilder.h>
#include <util/util.h>

#include <model/file.h>
#include <model/file-odb.hxx>

#include "csharpdiagram.h"

namespace
{

/**
 * This function checks if the given container contains a specified value.
 * @param v_ A container to inspect.
 * @param val_ Value which will be searched.
 * @return True if the container contains the value.
 */
template <typename Cont>
bool contains(const Cont& c_, const typename Cont::value_type& val_)
{
  return std::find(c_.begin(), c_.end(), val_) != c_.end();
}

/**
 * This function wraps the content to a HTML tag and adds attributes to it.
 */
std::string graphHtmlTag(
  const std::string& tag_,
  const std::string& content_,
  const std::string& attr_ = "")
{
  return std::string("<")
    .append(tag_)
    .append(" ")
    .append(attr_)
    .append(">")
    .append(content_)
    .append("</")
    .append(tag_)
    .append(">");
}

}

namespace cc
{
namespace service
{
namespace language
{

CsharpDiagram::CsharpDiagram(
  std::shared_ptr<odb::database> db_,
  std::shared_ptr<std::string> datadir_,
  const cc::webserver::ServerContext& context_)
    : _db(db_),
      _transaction(db_),
      _projectHandler(db_, datadir_, context_)
{
}

void CsharpDiagram::getClassCollaborationDiagram(
  util::Graph& graph_,
  const core::AstNodeId& astNodeId_)
{
  std::map<core::AstNodeId, util::Graph::Node> visitedNodes;
  std::map<GraphNodePair, util::Graph::Edge> visitedEdges;
  std::vector<AstNodeInfo> relatedNodes;
  std::vector<AstNodeInfo> nodes;

//   graph_.setAttribute("rankdir", "BT");

//   //--- Center node ---//

//   _cppHandler.getReferences(
//     nodes, astNodeId_, CppServiceHandler::DEFINITION, {});

//   if (nodes.empty())
//     return;

//   AstNodeInfo nodeInfo = nodes.front();

//   util::Graph::Node centerNode = addNode(graph_, nodeInfo);
//   decorateNode(graph_, centerNode, centerClassNodeDecoration);
//   visitedNodes[nodeInfo.id] = centerNode;
//   relatedNodes.push_back(nodeInfo);

//   nodes.clear();

//   //--- Types from which the queried type inherits ---//

//   _cppHandler.getReferences(nodes, nodeInfo.id,
//     CppServiceHandler::INHERIT_FROM, {});

//   for (const AstNodeInfo& node : nodes)
//   {
//     util::Graph::Node inheritNode = addNode(graph_, node);
//     graph_.setNodeAttribute(inheritNode, "label",
//       node.astNodeValue, true);
//     decorateNode(graph_, inheritNode, classNodeDecoration);

//     util::Graph::Edge edge = graph_.createEdge(centerNode, inheritNode);
//     decorateEdge(graph_, edge, inheritClassEdgeDecoration);

//     visitedNodes[node.id] = inheritNode;
//     relatedNodes.push_back(node);
//   }

//   nodes.clear();

//   //--- Types by which the queried type is inherited ---//

//   _cppHandler.getReferences(nodes, nodeInfo.id,
//     CppServiceHandler::INHERIT_BY, {});

//   for (const AstNodeInfo& node : nodes)
//   {
//     util::Graph::Node inheritNode = addNode(graph_, node);
//     graph_.setNodeAttribute(inheritNode, "label",
//       node.astNodeValue, true);
//     decorateNode(graph_, inheritNode, classNodeDecoration);

//     util::Graph::Edge edge = graph_.createEdge(inheritNode, centerNode);
//     decorateEdge(graph_, edge, inheritClassEdgeDecoration);

//     visitedNodes[node.id] = inheritNode;
//     relatedNodes.push_back(node);
//   }

//   //--- Get related types for the current and related types ---//

//   for (const AstNodeInfo& relatedNode : relatedNodes)
//   {
//     std::vector<AstNodeInfo> dataMembers;
//     _cppHandler.getReferences(dataMembers, relatedNode.id,
//       CppServiceHandler::DATA_MEMBER, {});

//     for (const AstNodeInfo& node : dataMembers)
//     {
//       std::vector<AstNodeInfo> types;
//       _cppHandler.getReferences(types, node.id, CppServiceHandler::TYPE, {});

//       if (types.empty())
//         continue;

//       AstNodeInfo typeInfo = types.front();

//       util::Graph::Node typeNode;
//       auto it = visitedNodes.find(typeInfo.id);
//       if (it == visitedNodes.end())
//       {
//         typeNode = addNode(graph_, typeInfo);
//         decorateNode(graph_, typeNode, classNodeDecoration);
//         visitedNodes.insert(it, std::make_pair(typeInfo.id, typeNode));
//       }
//       else
//         typeNode = it->second;

//       GraphNodePair graphEdge(visitedNodes[relatedNode.id], typeNode);
//       auto edgeIt = visitedEdges.find(graphEdge);
//       if (edgeIt == visitedEdges.end())
//       {
//         util::Graph::Edge edge =
//           graph_.createEdge(visitedNodes[relatedNode.id], typeNode);
//         decorateEdge(graph_, edge, usedClassEdgeDecoration);
//         graph_.setEdgeAttribute(edge, "label", node.astNodeValue);
//         visitedEdges.insert(edgeIt, std::make_pair(graphEdge, edge));
//       }
//       else
//       {
//         std::string oldLabel = graph_.getEdgeAttribute(edgeIt->second, "label");
//         graph_.setEdgeAttribute(edgeIt->second, "label",
//           oldLabel + ", " + node.astNodeValue);
//       }
//     }
//   }
}

void CsharpDiagram::getFunctionCallDiagram(
  util::Graph& graph_,
  const core::AstNodeId& astNodeId_)
{
  std::map<core::AstNodeId, util::Graph::Node> visitedNodes;
  std::vector<AstNodeInfo> nodes;

//   graph_.setAttribute("rankdir", "LR");

//   //--- Center node ---//

//   _cppHandler.getReferences(
//     nodes, astNodeId_, CppServiceHandler::DEFINITION, {});

//   if (nodes.empty())
//     return;

//   util::Graph::Node centerNode = addNode(graph_, nodes.front());
//   decorateNode(graph_, centerNode, centerNodeDecoration);
//   visitedNodes[astNodeId_] = centerNode;

//   //--- Callees ---//

//   nodes.clear();
//   _cppHandler.getReferences(nodes, astNodeId_, CppServiceHandler::CALLEE, {});

//   for (const AstNodeInfo& node : nodes)
//   {
//     util::Graph::Node calleeNode;

//     auto it = visitedNodes.find(node.id);
//     if (it == visitedNodes.end())
//     {
//       calleeNode = addNode(graph_, node);
//       decorateNode(graph_, calleeNode, calleeNodeDecoration);
//       visitedNodes.insert(it, std::make_pair(node.id, calleeNode));
//     }
//     else
//       calleeNode = it->second;

//     if (!graph_.hasEdge(centerNode, calleeNode))
//     {
//       util::Graph::Edge edge = graph_.createEdge(centerNode, calleeNode);
//       decorateEdge(graph_, edge, calleeEdgeDecoration);
//     }
//   }

//   //--- Callers ---//

//   nodes.clear();
//   _cppHandler.getReferences(nodes, astNodeId_, CppServiceHandler::CALLER, {});

//   for (const AstNodeInfo& node : nodes)
//   {
//     util::Graph::Node callerNode;

//     auto it = visitedNodes.find(node.id);
//     if (it == visitedNodes.end())
//     {
//       callerNode = addNode(graph_, node);
//       decorateNode(graph_, callerNode, callerNodeDecoration);
//       visitedNodes.insert(it, std::make_pair(node.id, callerNode));
//     }
//     else
//       callerNode = it->second;

//     if (!graph_.hasEdge(callerNode, centerNode))
//     {
//       util::Graph::Edge edge = graph_.createEdge(callerNode, centerNode);
//       decorateEdge(graph_, edge, callerEdgeDecoration);
//     }
//   }

//   _subgraphs.clear();
// }

// void CsharpDiagram::getDetailedClassDiagram(
//   util::Graph& graph_,
//   const core::AstNodeId& astNodeId_)
// {
//   std::vector<AstNodeInfo> nodes;

//   graph_.setAttribute("rankdir", "BT");

//   //--- Center node ---//

//   _cppHandler.getReferences(
//     nodes, astNodeId_, CppServiceHandler::DEFINITION, {});

//   if (nodes.empty())
//     return;

//   AstNodeInfo nodeInfo = nodes.front();

//   util::Graph::Node currentNode = addNode(graph_, nodeInfo);
//   graph_.setNodeAttribute(currentNode, "label",
//     getDetailedClassNodeLabel(nodeInfo), true);
//   graph_.setNodeAttribute(currentNode, "shape", "none");

//   nodes.clear();

//   //--- Types from which the queried type inherits ---//

//   _cppHandler.getReferences(nodes, nodeInfo.id,
//     CppServiceHandler::INHERIT_FROM, {});

//   for (const AstNodeInfo& node : nodes)
//   {
//     util::Graph::Node inheritNode = addNode(graph_, node);
//     graph_.setNodeAttribute(inheritNode, "label",
//       getDetailedClassNodeLabel(node), true);
//     graph_.setNodeAttribute(inheritNode, "shape", "none");

//     util::Graph::Edge edge = graph_.createEdge(currentNode, inheritNode);
//     decorateEdge(graph_, edge, inheritClassEdgeDecoration);
//   }

//   nodes.clear();

//   //--- Types by which the queried type is inherited ---//

//   _cppHandler.getReferences(nodes, nodeInfo.id,
//     CppServiceHandler::INHERIT_BY, {});

//   for (const AstNodeInfo& node : nodes)
//   {
//     util::Graph::Node inheritNode = addNode(graph_, node);
//     graph_.setNodeAttribute(inheritNode, "label",
//       getDetailedClassNodeLabel(node), true);
//     graph_.setNodeAttribute(inheritNode, "shape", "none");

//     util::Graph::Edge edge = graph_.createEdge(inheritNode, currentNode);
//     decorateEdge(graph_, edge, inheritClassEdgeDecoration);
//   }
}

std::string CsharpDiagram::getDetailedClassNodeLabel(const AstNodeInfo& nodeInfo_)
{
  std::string colAttr = "border='0' align='left'";
  std::string label = "<table border='1' cellspacing='0'>";

  label.append(graphHtmlTag("tr", graphHtmlTag("td",
    graphHtmlTag("font",
    util::escapeHtml(nodeInfo_.astNodeValue), "color='white'"),
    "colspan='2' SIDES='B' bgcolor='#316ECF' align='center'")));

  std::vector<AstNodeInfo> nodes;

//   //--- Data members of the class ---//

//   _cppHandler.getReferences(nodes, nodeInfo_.id,
//     CppServiceHandler::DATA_MEMBER, {});

//   for (auto it = nodes.begin(); it != nodes.end(); ++it)
//   {
//     std::string visibility = visibilityToHtml(*it);
//     std::string content = memberContentToHtml(*it,
//       util::escapeHtml(it->astNodeValue + " : " + getProperty(it->id, "Type")));

//     std::string attr = colAttr;
//     if (it == nodes.end() - 1)
//       attr = "border='1' align='left' SIDES='B'";

//     label += graphHtmlTag("tr",
//       graphHtmlTag("td", visibility, attr) +
//       graphHtmlTag("td", content, attr));
//   }

//   nodes.clear();

//   //--- Methods of the class ---//

//   _cppHandler.getReferences(nodes, nodeInfo_.id,
//     CppServiceHandler::METHOD, {});

//   for (const AstNodeInfo& node : nodes)
//   {
//     std::string visibility = visibilityToHtml(node);

//     // TODO: Constructor and Destructor signatures can be empty.
//     if (!node.astNodeValue.empty())
//     {
//       std::string content = memberContentToHtml(node,
//         util::escapeHtml(node.astNodeValue));

//       label += graphHtmlTag("tr",
//         graphHtmlTag("td", visibility, colAttr) +
//         graphHtmlTag("td", content, colAttr));
//     }
//   }

//   label.append("</table>");

   return label;
}

std::string CsharpDiagram::visibilityToHtml(const AstNodeInfo& node_)
{
  if (contains(node_.tags, "public"))
    return graphHtmlTag("font", "+", "color='green'");
  if (contains(node_.tags, "private"))
    return graphHtmlTag("font", "-", "color='red'");
  if (contains(node_.tags, "protected"))
    return graphHtmlTag("font", "#", "color='blue'");

  return "";
}

std::string CsharpDiagram::memberContentToHtml(
  const AstNodeInfo& node_,
  const std::string& content_)
{
  std::string startTags;
  std::string endTags;

  if (contains(node_.tags, "static"))
  {
    startTags += "<b>";
    endTags.insert(0, "</b>");
  }

  if (contains(node_.tags, "virtual"))
  {
    startTags += "<i>";
    endTags.insert(0, "</i>");
  }

  return startTags + util::escapeHtml(content_) + endTags;
}

// std::string CsharpDiagram::getProperty(
//   const core::AstNodeId& astNodeId_,
//   const std::string& property_)
// {
//   std::map<std::string, std::string> properties;
//   _cppHandler.getProperties(properties, astNodeId_);
//   return properties[property_];
// }

util::Graph::Node CsharpDiagram::addNode(
  util::Graph& graph_,
  const AstNodeInfo& nodeInfo_)
{
  util::Graph::Node node
    = graph_.getOrCreateNode(nodeInfo_.id,
      addSubgraph(graph_, nodeInfo_.range.file));

  graph_.setNodeAttribute(node, "label", nodeInfo_.astNodeValue);

  return node;
}

std::string CsharpDiagram::getDetailedClassLegend()
{
  util::LegendBuilder builder("Detailed Class CsharpDiagram");

  builder.addNode("class", classNodeDecoration);
  builder.addNode("public data member", {{"shape", "none"},
    {"label", graphHtmlTag("font", "+", "color='green'")}}, true);
  builder.addNode("private data member", {{"shape", "none"},
    {"label", graphHtmlTag("font", "-", "color='red'")}}, true);
  builder.addNode("protected data member", {{"shape", "none"},
    {"label", graphHtmlTag("font", "#", "color='blue'")}}, true);
  builder.addNode("static method or data member", {{"shape", "none"},
    {"label", "<b>static</b>"}}, true);
  builder.addNode("virtual method", {{"shape", "none"},
    {"label", "<i>virtual</i>"}}, true);
  builder.addEdge("inheritance", inheritClassEdgeDecoration);

  return builder.getOutput();
}

std::string CsharpDiagram::getFunctionCallLegend()
{
  util::LegendBuilder builder("Function Call CsharpDiagram");

  builder.addNode("center function", centerNodeDecoration);
  builder.addNode("called function", calleeNodeDecoration);
  builder.addNode("caller function", callerNodeDecoration);
  builder.addNode("virtual function", virtualNodeDecoration);
  builder.addEdge("called", calleeEdgeDecoration);
  builder.addEdge("caller", callerEdgeDecoration);

  return builder.getOutput();
}

std::string CsharpDiagram::getClassCollaborationLegend()
{
  util::LegendBuilder builder("Class Collaboration CsharpDiagram");

  builder.addNode("center class", centerClassNodeDecoration);
  builder.addNode("class", classNodeDecoration);
  builder.addEdge("contained or used class", usedClassEdgeDecoration);
  builder.addEdge("inheritance", inheritClassEdgeDecoration);

  return builder.getOutput();
}

util::Graph::Subgraph CsharpDiagram::addSubgraph(
  util::Graph& graph_,
  const core::FileId& fileId_)
{
  auto it = _subgraphs.find(fileId_);

  if (it != _subgraphs.end())
    return it->second;

  core::FileInfo fileInfo;
  _projectHandler.getFileInfo(fileInfo, fileId_);

  util::Graph::Subgraph subgraph
    = graph_.getOrCreateSubgraph("cluster_" + fileInfo.path);

  graph_.setSubgraphAttribute(subgraph, "id", fileInfo.id);
  graph_.setSubgraphAttribute(subgraph, "label", fileInfo.path);

  _subgraphs.insert(it, std::make_pair(fileInfo.path, subgraph));

  return subgraph;
}

void CsharpDiagram::decorateNode(
  util::Graph& graph_,
  const util::Graph::Node& node_,
  const Decoration& decoration_) const
{
  for (const auto& attr : decoration_)
    graph_.setNodeAttribute(node_, attr.first, attr.second);
}

void CsharpDiagram::decorateEdge(
  util::Graph& graph_,
  const util::Graph::Edge& edge_,
  const Decoration& decoration_) const
{
  for (const auto& attr : decoration_)
    graph_.setEdgeAttribute(edge_, attr.first, attr.second);
}

void CsharpDiagram::decorateSubgraph(
  util::Graph& graph_,
  const util::Graph::Subgraph& subgraph_,
  const Decoration& decoration_) const
{
  for (const auto& attr : decoration_)
    graph_.setSubgraphAttribute(subgraph_, attr.first, attr.second);
}

const CsharpDiagram::Decoration CsharpDiagram::centerNodeDecoration = {
  {"style", "filled"},
  {"fillcolor", "gold"}
};

const CsharpDiagram::Decoration CsharpDiagram::calleeNodeDecoration = {
  {"style", "filled"},
  {"fillcolor", "lightblue"}
};

const CsharpDiagram::Decoration CsharpDiagram::callerNodeDecoration = {
  {"style", "filled"},
  {"fillcolor", "coral"}
};

const CsharpDiagram::Decoration CsharpDiagram::virtualNodeDecoration = {
  {"shape", "diamond"},
  {"style", "filled"},
  {"fillcolor", "cyan"}
};

const CsharpDiagram::Decoration CsharpDiagram::calleeEdgeDecoration = {
  {"color", "blue"}
};

const CsharpDiagram::Decoration CsharpDiagram::callerEdgeDecoration = {
  {"color", "red"}
};

const CsharpDiagram::Decoration CsharpDiagram::centerClassNodeDecoration = {
  {"style", "filled"},
  {"fillcolor", "gold"},
  {"shape", "box"}
};

const CsharpDiagram::Decoration CsharpDiagram::classNodeDecoration = {
  {"shape", "box"}
};

const CsharpDiagram::Decoration CsharpDiagram::usedClassEdgeDecoration = {
  {"style", "dashed"},
  {"color", "mediumpurple"}
};

const CsharpDiagram::Decoration CsharpDiagram::inheritClassEdgeDecoration = {
  {"arrowhead", "empty"}
};

}
}
}
