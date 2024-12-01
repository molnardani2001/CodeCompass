#include <algorithm>
#include <regex>
#include <sstream>

#include <boost/filesystem.hpp>

#include <model/dependencyedge.h>
#include <model/dependencyedge-odb.hxx>
#include <model/msresource.h>
#include <model/msresource-odb.hxx>
#include <model/helmtemplate.h>
#include <model/helmtemplate-odb.hxx>

#include <model/file.h>
#include <service/yamlservice.h>
#include <util/dbutil.h>
#include <numeric>

#include "yamlfilediagram.h"

namespace cc
{
namespace service
{
namespace language
{

namespace fs = boost::filesystem;

typedef odb::query<model::DependencyEdge> EdgeQuery;
typedef odb::result<model::DependencyEdge> EdgeResult;
typedef odb::query<model::Microservice> MicroserviceQuery;
typedef odb::result<model::Microservice> MicroserviceResult;
typedef odb::query<model::MSResource> MSResourceQuery;
typedef odb::result<model::MSResource> MSResourceResult;
typedef odb::query<model::HelmTemplate> HelmTemplateQuery;
typedef odb::result<model::HelmTemplate> HelmTemplateResult;

YamlFileDiagram::YamlFileDiagram(
  std::shared_ptr<odb::database> db_,
  std::shared_ptr<std::string> datadir_,
  const cc::webserver::ServerContext& context_)
  : _db(db_),
    _transaction(db_),
    _yamlHandler(db_, datadir_, context_),
    _projectHandler(db_, datadir_, context_)
{
}

void YamlFileDiagram::getYamlFileInfo(
  util::Graph& graph_,
  const core::FileId& fileId_)
{
  std::string htmlContent = "";
    _transaction([&, this](){

      FilePathResult yamlPath = _db->query<model::File>(
    FilePathQuery::id == std::stoull(fileId_));

      YamlResult yamlInfo = _db->query<model::YamlFile>(
    YamlQuery::file == std::stoull(fileId_));

      YamlContentResult yamlContent = _db->query<model::YamlContent>(
    YamlContentQuery::file == std::stoull(fileId_));

      core::FileInfo fileInfo;
      _projectHandler.getFileInfo(fileInfo, fileId_);
      util::Graph::Node node = addNode(graph_, fileInfo);

      int numOfDataPairs = yamlContent.size();
      std::string type = typeToString(yamlInfo.begin()->type);
      std::string path = yamlPath.begin()->path;

      htmlContent +=  "<p><strong> FileType: </strong>" + type + "</p>";
      htmlContent += "<p><strong> FilePath: </strong>" + path + "</p>";
      htmlContent += "<p><strong> Key-data pairs: </strong>"
                     + std::to_string(numOfDataPairs) + "</p>";

      htmlContent += graphHtmlTag("p",
            graphHtmlTag("strong", "Main Keys:"));

      htmlContent += "<ul>";

      for (const model::YamlContent& yc : yamlContent)
      {
        htmlContent += graphHtmlTag("li", yc.key);
      }

      htmlContent += "</ul>";
      graph_.setNodeAttribute(node, "FileInfo", htmlContent, true);
  });
}


void YamlFileDiagram::getYamlFileDiagram(
  util::Graph& graph_,
  const core::FileId& fileId_)
{
  std::string thAttr
          = "style=\"background-color:lightGray; font-weight:bold; height:50px\"";

  std::string tdAttr = "style=\"height:30px\"";
  std::string table = "<table border='1' cellspacing='1' width='75%'>";

  _transaction([&, this](){
      typedef odb::result<model::YamlContent> YamlResult;
      typedef odb::query<model::YamlContent> YamlQuery;

      YamlResult yamlContent = _db->query<model::YamlContent>(
              YamlQuery::file == std::stoull(fileId_));

      core::FileInfo fileInfo;
      _projectHandler.getFileInfo(fileInfo, fileId_);
      util::Graph::Node node = addNode(graph_, fileInfo);

      table += graphHtmlTag("tr",
        graphHtmlTag("th", "Key", thAttr) +
        graphHtmlTag("th", "Value", thAttr));

      for (const model::YamlContent& yc : yamlContent)
      {
        table += graphHtmlTag("tr",
          graphHtmlTag("td", yc.key, tdAttr) +
          graphHtmlTag("td", yc.value, tdAttr));
      }
      table.append("</table>");

      graph_.setNodeAttribute(node, "content", table, true);
  });
}

void YamlFileDiagram::getMicroserviceDiagram(
  util::Graph& graph_,
  const core::FileId& fileId_)
{
  core::FileInfo fileInfo;
  _projectHandler.getFileInfo(fileInfo, fileId_);
  util::Graph::Node currentNode = addNode(graph_, fileInfo);

  util::bfsBuild(graph_, currentNode, std::bind(&YamlFileDiagram::getMicroservices,
    this, std::placeholders::_1, std::placeholders::_2),
    microserviceNodeDecoration, {}, 1);
}

std::vector<util::Graph::Node> YamlFileDiagram::getMicroservices(
  util::Graph& graph_,
  const util::Graph::Node& node_)
{
  std::vector<util::Graph::Node> microservices;

  _transaction([&, this] {
    for (const model::Microservice& service : _db->query<model::Microservice>())
    {
      microservices.push_back(addNode(graph_, service));
    }
  });

  return microservices;
}

/* ---- Dependent microservices ---- */

void YamlFileDiagram::getDependentServicesDiagram(
  util::Graph& graph_,
  const language::MicroserviceId& serviceId_)
{
  util::Graph::Node currentNode;
  model::Microservice::ServiceType nodeType;

  _transaction([&, this]{
    MicroserviceResult res = _db->query<model::Microservice>(
      MicroserviceQuery::serviceId == std::stoull(serviceId_));

    currentNode = addNode(graph_, *res.begin());
    nodeType = res.begin()->type;

    if (res.begin()->type == model::Microservice::ServiceType::PRODUCT)
      decorateNode(graph_, currentNode, {{"shape", "folder"}, {"fillcolor", "crimson"}});
    else if (res.begin()->type == model::Microservice::ServiceType::INTEGRATION)
      decorateNode(graph_, currentNode, {{"shape", "component"}, {"fillcolor", "dodgerblue3"}});
    else if (res.begin()->type == model::Microservice::ServiceType::CENTRAL)
      decorateNode(graph_, currentNode, {{"shape", "component"}, {"fillcolor", "aquamarine2"}});
  });

  if (nodeType == model::Microservice::ServiceType::PRODUCT)
  {
    util::bfsBuild(graph_, currentNode, std::bind(&YamlFileDiagram::getDependencies,
      this, std::placeholders::_1, std::placeholders::_2), {}, {});
  }
  else
  {
    std::vector<util::Graph::Node> serviceNodes = getDependencies(graph_, currentNode);
    std::vector<util::Graph::Node> revServiceNodes = getRevDependencies(graph_, currentNode);
  }
}

std::vector<util::Graph::Node> YamlFileDiagram::getDependencies(
  util::Graph& graph_,
  const util::Graph::Node& node_)
{
  return getDependentServices(graph_, node_);
}

std::vector<util::Graph::Node> YamlFileDiagram::getRevDependencies(
  util::Graph& graph_,
  const util::Graph::Node& node_)
{
  return getDependentServices(graph_, node_, true);
}

std::vector<util::Graph::Node> YamlFileDiagram::getDependentServices(
  util::Graph& graph_,
  const util::Graph::Node& node_,
  bool reverse_)
{
  std::vector<util::Graph::Node> dependencies;
  std::multimap<model::MicroserviceId, std::string> serviceIds = getDependentServiceIds(graph_, node_, reverse_);
  std::map<model::MicroserviceId, int> edgeCount;

  for (const auto& p : serviceIds)
  {
    if (edgeCount.find(p.first) != edgeCount.end())
      ++edgeCount.at(p.first);
    else
      edgeCount.insert(std::make_pair(p.first, 1));
  }

  for (const auto& serviceId : edgeCount)
  {
    _transaction([&, this]{
      MicroserviceResult res = _db->query<model::Microservice>(
        MicroserviceQuery::serviceId == serviceId.first);

      util::Graph::Node newNode = addNode(graph_, *res.begin());
      if (res.begin()->type == model::Microservice::ServiceType::PRODUCT)
        decorateNode(graph_, newNode, {{"shape", "folder"}, {"color", "crimson"}, {"fillcolor", "crimson"}});
      else if (res.begin()->type == model::Microservice::ServiceType::INTEGRATION)
        decorateNode(graph_, newNode, {{"shape", "component"}, {"color", "dodgerblue3"}, {"fillcolor", "dodgerblue3"}});
      else if (res.begin()->type == model::Microservice::ServiceType::CENTRAL)
        decorateNode(graph_, newNode, {{"shape", "component"}, {"color", "aquamarine2"}, {"fillcolor", "aquamarine2"}});

      dependencies.push_back(newNode);
      util::Graph::Edge edge;
      edge = reverse_ ? graph_.createEdge(newNode, node_) : graph_.createEdge(node_, newNode);
      decorateEdge(graph_, edge, {{"label", std::to_string(serviceId.second)}});

      if (graph_.hasEdge(newNode, node_) && graph_.hasEdge(node_, newNode))
        decorateEdge(graph_, edge, {{"color", "red"}});
    });
  }

  return dependencies;
}

std::multimap<model::MicroserviceId, std::string> YamlFileDiagram::getDependentServiceIds(
  util::Graph&,
  const util::Graph::Node& node_,
  bool reverse_) {
  std::multimap<model::MicroserviceId, std::string> dependencies;

  _transaction([&, this] {
    EdgeResult res = _db->query<model::DependencyEdge>(
      (reverse_
       ? EdgeQuery::to->serviceId
       : EdgeQuery::from->serviceId) == std::stoull(node_)
      && (EdgeQuery::type == "Service"));

    for (const model::DependencyEdge &edge: res)
    {
      model::MicroserviceId serviceId = reverse_ ? edge.from->serviceId : edge.to->serviceId;
      if (std::to_string(serviceId) != node_)
        dependencies.insert({serviceId, edge.type});
    }
  });

  return dependencies;
}

/* ---- Generated config maps ---- */

void YamlFileDiagram::getConfigMapsDiagram(
  util::Graph& graph_,
  const language::MicroserviceId& serviceId_)
{
  util::Graph::Node currentNode;

  _transaction([&, this]{
    MicroserviceResult res = _db->query<model::Microservice>(
      MicroserviceQuery::serviceId == std::stoull(serviceId_));

    currentNode = addNode(graph_, *res.begin());
  });

  std::vector<util::Graph::Node> configMapNodes = getConfigMaps(graph_, currentNode);
}

std::vector<util::Graph::Node> YamlFileDiagram::getConfigMaps(
  util::Graph& graph_,
  const util::Graph::Node& node_)
{
  return getDependentConfigMaps(graph_, node_);
}

std::vector<util::Graph::Node> YamlFileDiagram::getRevConfigMaps(
  util::Graph& graph_,
  const util::Graph::Node& node_)
{
  return getDependentConfigMaps(graph_, node_, true);
}

std::vector<util::Graph::Node> YamlFileDiagram::getDependentConfigMaps(
  util::Graph& graph_,
  const util::Graph::Node& node_,
  bool reverse_)
{
  std::vector<util::Graph::Node> dependencies;
  std::multimap<model::MicroserviceId, std::string> configMapIds = getDependentConfigMapIds(graph_, node_, reverse_);
  for (const auto& configMapId : configMapIds)
  {
    _transaction([&, this]{
      MicroserviceResult res = _db->query<model::Microservice>(
        MicroserviceQuery::serviceId == configMapId.first);

      util::Graph::Node newNode = addNode(graph_, *res.begin());
      dependencies.push_back(newNode);
      util::Graph::Edge edge = graph_.createEdge(node_, newNode);
      decorateEdge(graph_, edge, {{"label", configMapId.second}});
    });
  }

  return dependencies;
}

std::multimap<model::MicroserviceId, std::string> YamlFileDiagram::getDependentConfigMapIds(
  util::Graph& graph_,
  const util::Graph::Node& node_,
  bool reverse_)
{
  std::multimap<model::MicroserviceId, std::string> dependencies;

  _transaction([&, this]{
    EdgeResult res = _db->query<model::DependencyEdge>(
      (reverse_
       ? EdgeQuery::to->serviceId
       : EdgeQuery::from->serviceId) == std::stoull(node_)
      && (EdgeQuery::type == "ConfigMap"));

    for (const model::DependencyEdge& edge : res)
    {
      auto helm = _db->query_one<model::HelmTemplate>(
        HelmTemplateQuery::id == edge.connection->id);
      model::MicroserviceId serviceId = reverse_ ? edge.from->serviceId : edge.to->serviceId;
      dependencies.insert({serviceId, helm->name});
    }
  });

  return dependencies;
}

/* ---- Secrets ---- */

void YamlFileDiagram::getSecretsDiagram(
  util::Graph& graph_,
  const language::MicroserviceId& serviceId_)
{
  util::Graph::Node currentNode;

  _transaction([&, this]{
    MicroserviceResult res = _db->query<model::Microservice>(
      MicroserviceQuery::serviceId == std::stoull(serviceId_));

    currentNode = addNode(graph_, *res.begin());
  });

  std::vector<util::Graph::Node> secretNodes = getSecrets(graph_, currentNode);
}

std::vector<util::Graph::Node> YamlFileDiagram::getSecrets(
  util::Graph& graph_,
  const util::Graph::Node& node_)
{
  return getDependentSecrets(graph_, node_);
}

std::vector<util::Graph::Node> YamlFileDiagram::getRevSecrets(
  util::Graph& graph_,
  const util::Graph::Node& node_)
{
  return getDependentSecrets(graph_, node_, true);
}

std::vector<util::Graph::Node> YamlFileDiagram::getDependentSecrets(
  util::Graph& graph_,
  const util::Graph::Node& node_,
  bool reverse_)
{
  std::vector<util::Graph::Node> dependencies;
  std::multimap<model::MicroserviceId, std::string> secretIds = getDependentSecretIds(graph_, node_, reverse_);

  for (const auto& secretId : secretIds)
  {
    _transaction([&, this]{
      MicroserviceResult res = _db->query<model::Microservice>(
        MicroserviceQuery::serviceId == secretId.first);

      util::Graph::Node newNode = addNode(graph_, *res.begin());
      dependencies.push_back(newNode);
      util::Graph::Edge edge = graph_.createEdge(node_, newNode);
      decorateEdge(graph_, edge, {{"label", secretId.second}});
    });
  }

  return dependencies;
}

std::multimap<model::MicroserviceId, std::string> YamlFileDiagram::getDependentSecretIds(
  util::Graph&,
  const util::Graph::Node& node_,
  bool reverse_)
{
  std::multimap<model::MicroserviceId, std::string> dependencies;

  _transaction([&, this]{
    EdgeResult res = _db->query<model::DependencyEdge>(
      (reverse_
       ? EdgeQuery::to->serviceId
       : EdgeQuery::from->serviceId) == std::stoull(node_)
      && (EdgeQuery::type == "Secret"));

    for (const model::DependencyEdge& edge : res)
    {
      auto helm = _db->query_one<model::HelmTemplate>(
        HelmTemplateQuery::id == edge.connection->id);
      model::MicroserviceId serviceId = reverse_ ? edge.from->serviceId : edge.to->serviceId;
      dependencies.insert({serviceId, helm->name});
    }
  });

  return dependencies;
}

void YamlFileDiagram::getResourcesDiagram(
  util::Graph& graph_,
  const language::MicroserviceId& serviceId_)
{
  util::Graph::Node currentNode;

  _transaction([&, this]{
    MicroserviceResult res = _db->query<model::Microservice>(
      MicroserviceQuery::serviceId == std::stoull(serviceId_));

    currentNode = addNode(graph_, *res.begin());
  });

  std::vector<util::Graph::Node> secretNodes = getResources(graph_, currentNode);
}

std::vector<util::Graph::Node> YamlFileDiagram::getResources(
  util::Graph& graph_,
  const util::Graph::Node& node_)
{
  std::vector<util::Graph::Node> resources;

  _transaction([&, this]{
    MSResourceResult cpu = _db->query<model::MSResource>(
      MSResourceQuery::service == std::stoull(node_) &&
      MSResourceQuery::type == model::MSResource::ResourceType::CPU);

    float cpuSum = 0.0f;
    for (auto it = cpu.begin(); it != cpu.end(); ++it)
      cpuSum += it->amount;

    util::Graph::Node cpuNode = addNode(graph_, model::MSResource::ResourceType::CPU, cpuSum);
    resources.push_back(cpuNode);
    util::Graph::Edge cpuEdge = graph_.createEdge(node_, cpuNode);
    decorateEdge(graph_, cpuEdge, {{"label", resourceTypeToString(model::MSResource::ResourceType::CPU)}});

    MSResourceResult memory = _db->query<model::MSResource>(
      MSResourceQuery::service == std::stoull(node_) &&
      MSResourceQuery::type == model::MSResource::ResourceType::MEMORY);

    float memorySum = 0.0f;
    for (auto it = memory.begin(); it != memory.end(); ++it)
      memorySum += it->amount;

    util::Graph::Node memoryNode = addNode(graph_, model::MSResource::ResourceType::MEMORY, memorySum);
    resources.push_back(memoryNode);
    util::Graph::Edge memoryEdge = graph_.createEdge(node_, memoryNode);
    decorateEdge(graph_, memoryEdge, {{"label", resourceTypeToString(model::MSResource::ResourceType::MEMORY)}});

    MSResourceResult storage = _db->query<model::MSResource>(
      MSResourceQuery::service == std::stoull(node_) &&
      MSResourceQuery::type == model::MSResource::ResourceType::STORAGE);

    float storageSum = 0.0f;
    for (auto it = storage.begin(); it != storage.end(); ++it)
      storageSum += it->amount;

    util::Graph::Node storageNode = addNode(graph_, model::MSResource::ResourceType::STORAGE, storageSum);
    resources.push_back(storageNode);
    util::Graph::Edge storageEdge = graph_.createEdge(node_, storageNode);
    decorateEdge(graph_, storageEdge, {{"label", resourceTypeToString(model::MSResource::ResourceType::STORAGE)}});
  });

  return resources;
}

void YamlFileDiagram::depthFirstSearch(
  util::Graph::Node& node,
  std::unordered_set<util::Graph::Node>& visited,
  std::vector<util::Graph::Node>& path,
  std::vector<std::vector<util::Graph::Node>>& circles,
  util::Graph& graph)
{
  /*visited.insert(node);
  path.push_back(node);

  for (int neighbor : node.) {
    if (visited.find(neighbor) == visited.end()) {
      depthFirstSearch(neighbor, visited, path, circles, graph);
    }
    else if (find(path.begin(), path.end(), neighbor) != path.end()) {
      vector<int> circle(path.begin() + distance(path.begin(), find(path.begin(), path.end(), neighbor)), path.end());
      circles.push_back(circle);
    }
  }

  path.pop_back();*/
}

std::string YamlFileDiagram::graphHtmlTag(
  const std::string& tag_,
  const std::string& content_,
  const std::string& attr_)
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

util::Graph::Node YamlFileDiagram::addNode(
  util::Graph& graph_,
  const core::FileInfo& fileInfo_)
{
  util::Graph::Node node_ = graph_.getOrCreateNode(fileInfo_.id);
  graph_.setNodeAttribute(node_, "label", getLastNParts(fileInfo_.path, 3));

  if (fileInfo_.type == model::File::DIRECTORY_TYPE)
  {
    decorateNode(graph_, node_, directoryNodeDecoration);
  }
  else if (fileInfo_.type == model::File::BINARY_TYPE)
  {
    decorateNode(graph_, node_, binaryFileNodeDecoration);
  }
  else
  {
    std::string ext = boost::filesystem::extension(fileInfo_.path);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".yaml" || ext == ".yml")
      decorateNode(graph_, node_, sourceFileNodeDecoration);
  }

  return node_;
}

util::Graph::Node YamlFileDiagram::addNode(
  util::Graph& graph_,
  const model::Microservice& service_)
{
  util::Graph::Node node_ = graph_.getOrCreateNode(std::to_string(service_.serviceId));
  graph_.setNodeAttribute(node_, "label", service_.name + "\n" + service_.version);

  //decorateNode(graph_, node_, microserviceNodeDecoration);
  if (service_.type == model::Microservice::ServiceType::PRODUCT)
    decorateNode(graph_, node_, {{"shape", "folder"}, {"style", "filled"}, {"fillcolor", "crimson"}, {"fontcolor", "white"}});
  else if (service_.type == model::Microservice::ServiceType::INTEGRATION)
    decorateNode(graph_, node_, {{"shape", "component"}, {"style", "filled"}, {"fillcolor", "dodgerblue"}, {"fontcolor", "white"}});
  else if (service_.type == model::Microservice::ServiceType::CENTRAL)
    decorateNode(graph_, node_, {{"shape", "component"}, {"style", "filled"}, {"fillcolor", "aquamarine2"}});

  return node_;
}

util::Graph::Node YamlFileDiagram::addNode(
  util::Graph& graph_,
  const model::MSResource::ResourceType& type_,
  float amount_)
{
  util::Graph::Node node_ = graph_.getOrCreateNode(resourceTypeToString(type_));
  graph_.setNodeAttribute(node_, "label", std::to_string(std::ceil(amount_ * 100.0) / 100.0));

  decorateNode(graph_, node_, sourceFileNodeDecoration);


  return node_;
}

std::string YamlFileDiagram::getLastNParts(
  const std::string& path_,
  std::size_t n_)
{
  if (path_.empty() || n_ == 0)
    return "";

  std::size_t p;
  for (p = path_.rfind('/');
       --n_ > 0 && p - 1 < path_.size();
       p = path_.rfind('/', p - 1));

  return p > 0 && p < path_.size() ? "..." + path_.substr(p) : path_;
}

void YamlFileDiagram::decorateNode(
        util::Graph& graph_,
        const util::Graph::Node& node_,
        const Decoration& decoration_) const
{
  for (const auto& attr : decoration_)
    graph_.setNodeAttribute(node_, attr.first, attr.second);
}

void YamlFileDiagram::decorateEdge(
  util::Graph& graph_,
  const util::Graph::Edge& edge_,
  const Decoration& decoration_) const
{
  for (const auto& attr : decoration_)
    graph_.setEdgeAttribute(edge_, attr.first, attr.second);
}

const YamlFileDiagram::Decoration
  YamlFileDiagram::sourceFileNodeDecoration = {
  {"shape", "box"},
  {"style", "filled"},
  {"fillcolor", "#116db6"},
  {"fontcolor", "white"}
};

const YamlFileDiagram::Decoration
        YamlFileDiagram::directoryNodeDecoration = {
  {"shape", "folder"}
};

const YamlFileDiagram::Decoration
  YamlFileDiagram::binaryFileNodeDecoration = {
  {"shape", "box3d"},
  {"style", "filled"},
  {"fillcolor", "#f18a21"},
  {"fontcolor", "white"}
};

const YamlFileDiagram::Decoration YamlFileDiagram::microserviceNodeDecoration = {
  {"shape", "folder"},
  {"color", "blue"}
};

const YamlFileDiagram::Decoration YamlFileDiagram::dependsEdgeDecoration = {
  {"label", "depends on"}
};

}
}
}