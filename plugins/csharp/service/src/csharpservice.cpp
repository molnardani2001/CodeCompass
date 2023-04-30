#include <service/csharpservice.h>
#include <util/dbutil.h>
#include <util/util.h>
#include <model/file.h>
#include <model/file-odb.hxx>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

#include "csharpfilediagram.h"
#include "csharpdiagram.h"

namespace cc
{
namespace service
{
namespace language
{
typedef odb::query<cc::model::File> FileQuery;
typedef odb::result<model::File> FileResult;
typedef std::map<std::string, std::vector<std::string>> Usages;
typedef std::map<std::string, std::vector<Usages>> DirectoryUsages;
namespace fs = boost::filesystem;
namespace bp = boost::process;
namespace pt = boost::property_tree;

int CsharpServiceHandler::_thriftServerPort = 9091;

CsharpServiceHandler::CsharpServiceHandler(
  std::shared_ptr<odb::database> db_,
  std::shared_ptr<std::string> datadir_,
  const cc::webserver::ServerContext& context_)
    : _db(db_),
      _transaction(db_),
      _datadir(datadir_),
      _context(context_)
{
  _csharpQueryHandler.setThriftServerPort(_thriftServerPort);
  fs::path csharp_path =
   fs::system_complete("../lib/serviceplugin/csharpservice/");

  std::string command("./csharpservice ");
  command.append(getDbString());
  command.append(" ");
  command.append(std::to_string(_thriftServerPort));
  ++_thriftServerPort;
  c = bp::child(bp::start_dir(csharp_path), command);
  try
  {
    _csharpQueryHandler.getClientInterface(25000);
  }
  catch (TransportException& ex)
  {
    LOG(error) << "[csharpservice] Starting service failed!";
  }
}

std::string CsharpServiceHandler::getDbString()
{
  pt::ptree _pt;
  pt::read_json(*_datadir + "/project_info.json", _pt);

  return _pt.get<std::string>("database");
}

void CsharpServiceHandler::getFileTypes(std::vector<std::string>& return_)
{
  //LOG(info) << "CsharpServiceHandler getFileTypes";
  return_.push_back("CS");
  return_.push_back("Dir");
}

void CsharpServiceHandler::getAstNodeInfo(
        AstNodeInfo& return_,
        const core::AstNodeId& astNodeId_)
{
  _csharpQueryHandler.getAstNodeInfo(return_, astNodeId_);
  model::FilePtr file = _transaction([&, this](){
    return _db->query_one<model::File>(
      FileQuery::path == return_.range.file);
  });
  std::stringstream ss;
  ss << file->id; // itt csak siman ss<<file volt;
  return_.range.file = ss.str();
  LOG(info) << "csharpQuery.getAstNodeInfo: file = " << return_.range.file;
}

void CsharpServiceHandler::getAstNodeInfoByPosition(
        AstNodeInfo& return_,
        const core::FilePosition& fpos_)
{
  model::FilePtr file = _transaction([&, this](){
    return _db->query_one<model::File>(
      FileQuery::id == std::stoull(fpos_.file));
  });
  _csharpQueryHandler.getAstNodeInfoByPosition(return_, file->path, fpos_.pos);
}

void CsharpServiceHandler::getSourceText(
        std::string& return_,
        const core::AstNodeId& astNodeId_)
{
  LOG(info) << "getSourceText";
  core::FileRange fileRange;

  _csharpQueryHandler.getFileRange(fileRange, astNodeId_);

  return_ = _transaction([&, this](){
      model::FilePtr file = _db->query_one<model::File>(
              FileQuery::id == std::stoull(fileRange.file));

      if (!file) {
        return std::string();
      }

      return cc::util::textRange(
              file->content.load()->content,
              fileRange.range.startpos.line,
              fileRange.range.startpos.column,
              fileRange.range.endpos.line,
              fileRange.range.endpos.column);
  });
}

void CsharpServiceHandler::getProperties(
        std::map<std::string, std::string>& return_,
        const core::AstNodeId& astNodeId_)
{
  //LOG(info) << "getProperties";
  _csharpQueryHandler.getProperties(return_, astNodeId_);
}

void CsharpServiceHandler::getDocumentation(
        std::string& return_,
        const core::AstNodeId& astNodeId_)
{
  LOG(info) << "getDocumentation";
  _csharpQueryHandler.getDocumentation(return_, astNodeId_);
}

void CsharpServiceHandler::getDiagramTypes(
        std::map<std::string, std::int32_t>& return_,
        const core::AstNodeId& astNodeId_)
{
  LOG(info) << "getDiagramTypes";
  _csharpQueryHandler.getDiagramTypes(return_, astNodeId_); 
}

void CsharpServiceHandler::getDiagram(
        std::string& return_,
        const core::AstNodeId& astNodeId_,
        const std::int32_t diagramId_)
{
  LOG(info) << "getDiagram";
  CsharpDiagram diagram(_db,_datadir,_context);
  util::Graph graph;

  switch (diagramId_)
  {
    case FUNCTION_CALL:
    {
      // Center node 
      AstNodeInfo centerInfo;
      getAstNodeInfo(centerInfo,astNodeId_);
      //logging purposes
      LOG(info) << "centerastnodeinfo: ";
      LOG(info) << "astnodetype: " << centerInfo.astNodeType;
      LOG(info) << "value: " << centerInfo.astNodeValue;
      LOG(info) << "entityhash: " << centerInfo.entityHash;
      LOG(info) << "symboltype: " << centerInfo.symbolType;

      // Callee nodes
      std::vector<AstNodeInfo> calleeInfos;
      getReferences(calleeInfos,astNodeId_,(int)ReferenceType::CALLEE,{});
      //logging purposes
      for (const auto& info : calleeInfos){
        LOG(info) << "CalleeInfo: " << info.astNodeValue;
      }

      // Caller nodes
      std::vector<AstNodeInfo> callerInfos;
      getReferences(callerInfos,astNodeId_,(int)ReferenceType::CALLER,{});
      //logging purposes
      for (const auto& info : calleeInfos){
        LOG(info) << "CallerInfo: " << info.astNodeValue;
      }

      diagram.getFunctionCallDiagram(graph,centerInfo,calleeInfos,callerInfos);
      break;
    }
    case DETAILED_CLASS:
    { 
      // Center node 
      AstNodeInfo centInfo;
      getAstNodeInfo(centInfo,astNodeId_);

      //Data members - property nodes
      std::vector<AstNodeInfo> propertyInfos;
      getReferences(propertyInfos, astNodeId_,(int)ReferenceType::DATA_MEMBER,{});
      for (const auto& info : propertyInfos){
        LOG(info) << "PropertyInfo: " << info.astNodeValue;
      }

      // Method nodes
      std::vector<AstNodeInfo> methodInfos;
      getReferences(methodInfos,astNodeId_,(int)ReferenceType::METHOD,{});
      for (const auto& info : methodInfos){
        LOG(info) << "MethodInfo: " << info.astNodeValue;
      }

      diagram.getDetailedClassDiagram(graph,centInfo,propertyInfos,methodInfos);
      break;
    }
  }


  if (graph.nodeCount() != 0)
    return_ = graph.output(util::Graph::SVG);
  //_csharpQueryHandler.getDiagram(return_, astNodeId_, diagramId_);
}

void CsharpServiceHandler::getDiagramLegend(
        std::string& return_,
        const std::int32_t diagramId_)
{
  LOG(info) << "getDiagramLegend";

  CsharpDiagram diagram(_db,_datadir, _context);

  switch (diagramId_)
  {
    case FUNCTION_CALL:
      return_ = diagram.getFunctionCallLegend();
      break;

    case DETAILED_CLASS:
      return_ = diagram.getDetailedClassLegend();
      break;
  }
}

void CsharpServiceHandler::getFileDiagramTypes(
        std::map<std::string, std::int32_t>& return_,
        const core::FileId& fileId_)
{
  LOG(info) << "getFileDiagramTypes";
  //_csharpQueryHandler.getFileDiagramTypes(return_, fileId_); //most irtam

  model::FilePtr file = _transaction([&, this](){
    return _db->query_one<model::File>(
      FileQuery::id == std::stoull(fileId_));
  });

  if (file)
  {
    if (file->type == model::File::DIRECTORY_TYPE)
    {
      return_["Internal architecture of this module"] = SUBSYSTEM_DEPENDENCY;
      return_["Users of this module"]                 = EXTERNAL_USERS;
    }
    else if (file->type == "CS")
    {
      return_["File usage"] = FILE_USAGES;
    }
  }
}

void CsharpServiceHandler::getFileDiagram(
        std::string& return_,
        const core::FileId& fileId_,
        const int32_t diagramId_)
{
  LOG(info) << "getFileDiagram";

  CsharpFileDiagram diagram(_db,_datadir, _context);
  util::Graph graph;
  graph.setAttribute("rankdir", "LR");

  switch (diagramId_){
    case FILE_USAGES: //FILE_USAGES
    {
      Usages useIds;
      _csharpQueryHandler.getFileUsages(useIds,fileId_,false);
      // for (const auto& entry : data){
      //   LOG(info) << "Key: " << entry.first;
      //   LOG(info) << "Value:";
      //   for (const auto& value : entry.second){
      //     LOG(info) << value;
      //   }
      // }
      Usages revUseIds;
      _csharpQueryHandler.getFileUsages(revUseIds,fileId_,true);

      diagram.getFileUsagesDiagram(graph,fileId_,useIds,revUseIds);

      break;
    }
    case SUBSYSTEM_DEPENDENCY:
    {
      diagram.getSubsystemDependencyDiagram(graph,fileId_);
      break;
    }
    case EXTERNAL_USERS:
    {
      DirectoryUsages dirRevUsages; 

      //This is the main directory to which the diagram is called upon.
      model::FilePtr directory = _transaction([&,this]{
          return _db->query_one<model::File>(
            FileQuery::id == std::stoull(fileId_)
          );
      });
      LOG(info) << "MAIN DIR PATH" << directory->path;

      //This will contain all the directories from the project
      std::vector<model::FilePtr> directories;
      util::OdbTransaction {_db} ([&]() {
        FileResult res = _db->query<model::File>(
          FileQuery::type == model::File::DIRECTORY_TYPE);
        for (auto fp : res)
          directories.push_back(std::make_shared<model::File>(std::move(fp)));
      });

      //Sub will contain all the directories which are subdirectories
      //of main directory, including the main directory.
      //Checking is done with the paths.
      std::vector<model::FilePtr> sub; 
      sub.push_back(directory);
      for (const auto& dir : directories)
      {
        if (dir->path.find(directory->path + '/') != std::string::npos) 
          sub.push_back(dir);
      }

      //LOGGING: check fetched directories
      for (const auto& s : sub)
      {
        LOG(info) << s->path;
      }

      // Iterate on subddirectories and add a BFS to each CS file
      // under the subdir.
      // Gather for each subdir every BFS for every CS file into
      // dirRevUsages.
      std::vector<model::FilePtr> css;
      std::vector<Usages> revs;
      Usages rev;
      for (const model::FilePtr& subdir : sub)
      {
        util::OdbTransaction {_db} ([&](){
          FileResult res = _db->query<model::File>(
                FileQuery::parent == subdir->id &&
                FileQuery::type == "CS");
          for (auto fp : res)
            css.push_back(std::make_shared<model::File>(std::move(fp)));
        });
        for (const model::FilePtr& cs : css)
        {
          std::string id = std::to_string(cs->id);
          LOG(info) << "Converted CS ID: " << id;
          _csharpQueryHandler.getFileUsages(rev,id,true);
          revs.push_back(rev);
          rev.clear();
        }
        dirRevUsages[std::to_string(subdir->id)] = revs;
        revs.clear();
        css.clear();
      }
      diagram.getExternalUsersDiagram(graph,fileId_,dirRevUsages);
      break;
    }
  }

  if (graph.nodeCount() != 0)
    return_ = graph.output(util::Graph::SVG);
}

void CsharpServiceHandler::getFileDiagramLegend(
        std::string& return_,
        const std::int32_t diagramId_)
{
  LOG(info) << "getFileDiagramLegend";
  CsharpFileDiagram diagram(_db, _datadir, _context);

  switch (diagramId_)
  {

      case EXTERNAL_USERS:
        return_ = diagram.getExternalUsersDiagramLegend();
        break;

      case FILE_USAGES:
        return_ = diagram.getFileUsagesDiagramLegend();
        break;

      case SUBSYSTEM_DEPENDENCY:
        return_ = diagram.getSubsystemDependencyDiagramLegend();
        break;
  }
}

void CsharpServiceHandler::getReferenceTypes(
        std::map<std::string, std::int32_t>& return_,
        const core::AstNodeId& astNodeId_)
{
  //LOG(info) << "getReferenceTypes";
  _csharpQueryHandler.getReferenceTypes(return_, astNodeId_);
}

std::int32_t CsharpServiceHandler::getReferenceCount(
        const core::AstNodeId& astNodeId_,
        const std::int32_t referenceId_)
{
  //LOG(info) << "getReferenceCount";
  return _csharpQueryHandler.getReferenceCount(astNodeId_, referenceId_);
}

void CsharpServiceHandler::getReferences(
        std::vector<AstNodeInfo>& return_,
        const core::AstNodeId& astNodeId_,
        const std::int32_t referenceId_,
        const std::vector<std::string>& tags_)
{
  LOG(info) << "getReferences";
  _csharpQueryHandler.getReferences(return_, astNodeId_, referenceId_, tags_);
  std::vector<AstNodeInfo> ret;
  for (AstNodeInfo nodeinfo : return_)
  {
    model::FilePtr file = _transaction([&, this](){
    return _db->query_one<model::File>(
    FileQuery::path == nodeinfo.range.file);
    });
    LOG(info) << "getreferences: nodeInfo: " << nodeinfo.astNodeValue << " | " << nodeinfo.astNodeType;
    LOG(info) << "getreferences: file: " << file->filename;
    
    std::stringstream ss;
    ss << file->id;
    nodeinfo.range.file = ss.str();
    ret.push_back(nodeinfo);
  }
  return_ = ret;
}

void CsharpServiceHandler::getReferencesInFile(
        std::vector<AstNodeInfo>& /* return_ */,
        const core::AstNodeId& /* astNodeId_ */,
        const std::int32_t /* referenceId_ */,
        const core::FileId& /* fileId_ */,
        const std::vector<std::string>& /* tags_ */)
{
  //LOG(info) << "getReferencesInFile";
  // TODO
}

void CsharpServiceHandler::getReferencesPage(
        std::vector<AstNodeInfo>& /* return_ */,
        const core::AstNodeId& /* astNodeId_ */,
        const std::int32_t /* referenceId_ */,
        const std::int32_t /* pageSize_ */,
        const std::int32_t /* pageNo_ */)
{
  //LOG(info) << "getReferencesPage";
  // TODO
}

void CsharpServiceHandler::getFileReferenceTypes(
        std::map<std::string, std::int32_t>& return_,
        const core::FileId& /* fileId_*/)
{
  //LOG(info) << "getFileReferenceTypes";
  _csharpQueryHandler.getFileReferenceTypes(return_);
}

std::int32_t CsharpServiceHandler::getFileReferenceCount(
        const core::FileId& fileId_,
        const std::int32_t referenceId_)
{
  //LOG(info) << "getFileReferenceCount";
  model::FilePtr file = _transaction([&, this](){
    return _db->query_one<model::File>(
      FileQuery::id == std::stoull(fileId_));
  });
  return _csharpQueryHandler.getFileReferenceCount(file->path, referenceId_);
}

void CsharpServiceHandler::getFileReferences(
        std::vector<AstNodeInfo>& return_,
        const core::FileId& fileId_,
        const std::int32_t referenceId_)
{
  //LOG(info) << "getFileReferences";
  model::FilePtr file = _transaction([&, this](){
    return _db->query_one<model::File>(
      FileQuery::id == std::stoull(fileId_));
  });
  _csharpQueryHandler.getFileReferences(return_, file->path, referenceId_);
}

void CsharpServiceHandler::getSyntaxHighlight(
        std::vector<SyntaxHighlight>& return_,
        const core::FileRange& range_)
{
  LOG(info) << "getSyntaxHighlight";
  /*
  std::vector<std::string> content;
  _transaction([&, this]() {
      //--- Load the file content and break it into lines ---//
      model::FilePtr file = _db->query_one<model::File>(
        FileQuery::id == std::stoull(range_.file));
      if (!file || !file->content.load())
        return;
      std::istringstream s(file->content->content);
      std::string line;
      while (std::getline(s, line))
        content.push_back(line);
  });
  _csharpQueryHandler.getSyntaxHighlight(return_, range_, content);
  */
}

} // language

namespace csharp
{
std::stringstream CSharpQueryHandler::_thriftStream;
int CSharpQueryHandler::_thriftServerPort;
}
} // service
} // cc
