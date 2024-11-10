#include <iterator>
#include <fstream>
#include <memory>
#include <functional>
#include <regex>

#include "yaml-cpp/yaml.h"

#include <boost/filesystem.hpp>

#include <util/logutil.h>
#include <util/dbutil.h>
#include <util/odbtransaction.h>
#include <util/threadpool.h>
#include <util/util.h>

#include <parser/sourcemanager.h>

#include <model/buildlog.h>
#include <model/buildlog-odb.hxx>
#include <model/file.h>
#include <model/file-odb.hxx>

#include <model/microservice.h>
#include <model/microservice-odb.hxx>
#include <model/yamlfile.h>
#include <model/yamlfile-odb.hxx>
#include <model/yamlcontent.h>
#include <model/yamlcontent-odb.hxx>
#include <model/yamlastnode.h>
#include <model/yamlastnode-odb.hxx>

#include "parser/yamlparser.h"
#include "valueanalyzer.h"
#include "templateanalyzer.h"

namespace cc
{
namespace parser
{

namespace fs = boost::filesystem;

YamlParser::YamlParser(ParserContext& ctx_): AbstractParser(ctx_)
{
  _areDependenciesListed = true;

  util::OdbTransaction {_ctx.db} ([&, this] {
    for (const model::YamlFile& yf
            : _ctx.db->query<model::YamlFile>())
    {
      _fileIdCache.insert(yf.file);
    }
  });

  int threadNum = _ctx.options["jobs"].as<int>();
  _pool = util::make_thread_pool<std::string>(
  threadNum, [&, this](const std::string& path_)
  {
    LOG(info) << "[yamlparser] Processing " << path_;
    model::FilePtr file = _ctx.srcMgr.getFile(path_);
    if (file)
    {
      if (_fileIdCache.find(file->id) == _fileIdCache.end())
      {
        if (accept(file->path))
        {
          bool success = collectAstNodes(file);
          ++this->_visitedFileCount;
          file->parseStatus = success
            ? model::File::PSFullyParsed
            : model::File::PSPartiallyParsed;
          file->type = "YAML";
          _ctx.srcMgr.updateFile(*file);
        }
      }
      else
        LOG(debug) << "YamlParser already parsed this file: " << file->path;
    }
  });

}

bool YamlParser::cleanupDatabase()
{
  if (!_fileIdCache.empty())
  {
    try
    {
      util::OdbTransaction {_ctx.db} ([this] {
        for (const model::File& file
          : _ctx.db->query<model::File>(
          odb::query<model::File>::id.in_range(
            _fileIdCache.begin(), _fileIdCache.end())))
        {
          auto it = _ctx.fileStatus.find(file.path);
          if (it != _ctx.fileStatus.end() &&
            (it->second == cc::parser::IncrementalStatus::DELETED ||
             it->second == cc::parser::IncrementalStatus::MODIFIED ||
             it->second == cc::parser::IncrementalStatus::ACTION_CHANGED))
          {
            LOG(info) << "[yamlparser] Database cleanup: " << file.path;

            _ctx.db->erase_query<model::YamlFile>(
              odb::query<model::YamlFile>::file == file.id);
            _fileIdCache.erase(file.id);
          }
        }
      });
    }
    catch (odb::database_exception&)
    {
      LOG(fatal) << "[yamlparser] Transaction failed!";
      return false;
    }
  }
  return true;
}

bool YamlParser::parse()
{
  this->_visitedFileCount = 0;

  for (const std::string& input :
    _ctx.options["input"].as<std::vector<std::string>>())
  {
    if (fs::is_directory(input))
    {
      LOG(info) << "[yamlparser]  Yaml parse path: " << input;

      //--- Parse YAML files ---//

      util::OdbTransaction trans(_ctx.db);
      trans([&, this]() {
        auto cb = getParserCallback();
        /*--- Call non-empty iter-callback for all files
           in the current root directory. ---*/
        try
        {
          util::iterateDirectoryRecursive(input, cb);
        }
        catch (std::exception& ex_)
        {
          LOG(warning)
            << "[yamlparser] Yaml parser threw an exception: " << ex_.what();
        }
        catch (...)
        {
          LOG(warning)
            << "[yamlparser]  Yaml parser failed with unknown exception!";
        }
      });
    }
  }
  _pool->wait();
  LOG(info) << "[yamlparser] Processed files: " << this->_visitedFileCount;

  _ctx.srcMgr.persistFiles();

  //--- Collect relations ---/
  TemplateAnalyzer templateAnalyzer(_ctx, _fileAstCache);
  templateAnalyzer.init();

  ValueAnalyzer relationCollector(_ctx, _valuesAstCache, templateAnalyzer.getTemplateCounter());
  relationCollector.init();

  return true;
}

util::DirIterCallback YamlParser::getParserCallback()
{
  return [this](const std::string& currPath_)
  {
    boost::filesystem::path path(currPath_);

    if (boost::filesystem::is_regular_file(path))
    {
      _pool->enqueue(currPath_);
    }

    return true;
  };
}

bool YamlParser::accept(const std::string& path_) const
{
  std::string ext = boost::filesystem::extension(path_);
  return ext == ".yaml" || ext == ".yml";
}

void YamlParser::processFileType(model::FilePtr& file_, YAML::Node& loadedFile)
{
  util::OdbTransaction {_ctx.db} ([&] {
    model::YamlFilePtr file = std::make_shared<model::YamlFile>();
    file->file = file_->id;

    if (file_->filename == "Chart.yaml" || file_->filename == "Chart.yml")
    {
      if (file_->path.find("charts/") != std::string::npos)
      {
        file->type = model::YamlFile::Type::HELM_SUBCHART;
        
        if (std::find(_processedMS.begin(), _processedMS.end(), YAML::Dump(loadedFile["name"])) == _processedMS.end()
          && std::find(_processedMS.begin(), _processedMS.end(), YAML::Dump(loadedFile["version"])) == _processedMS.end())
        {
          model::Microservice service;
          service.file = file_->id;
          service.name = YAML::Dump(loadedFile["name"]);
          service.type = model::Microservice::ServiceType::INTEGRATION;
          service.serviceId = cc::model::createIdentifier(service);
          service.version = YAML::Dump(loadedFile["version"]);
          _ctx.db->persist(service);
          _processedMS.push_back(service.name);
        }
      }
      else
      {
        file->type = model::YamlFile::Type::HELM_CHART;
        processIntegrationChart(file_, loadedFile);
      }

      _mutex.lock();
      _fileAstCache.insert({file_->path, loadedFile});
      _mutex.unlock();
    }
    else if (file_->filename == "values.yaml" || file_->filename == "values.yml")
    {
      file->type = model::YamlFile::Type::HELM_VALUES;

      // If a values.yaml file belongs to a subchart in the chart set,
      // it should not be parsed since it contains default values
      // that may provide false results in the microservice architecture
      // dependency mapping.
      _mutex.lock();
      _valuesAstCache.insert({file_->path, loadedFile});
      _mutex.unlock();
      /*if (file_->path.find("charts/") == std::string::npos)
      {
        model::Microservice service;
        service.file = file_->id;
        service.name = fs::path(file_->path).parent_path().filename().string();
        service.serviceId = cc::model::createIdentifier(service);
        _ctx.db->persist(service);
      }*/
    }
    else if (file_->path.find("templates/") != std::string::npos)
    {
      file->type = model::YamlFile::Type::HELM_TEMPLATE;

      _mutex.lock();
      _fileAstCache.insert({file_->path, loadedFile});
      _mutex.unlock();
    }
    else if (file_->filename == "compose.yaml" || file_->filename == "compose.yml"
          || file_->filename == "docker-compose.yaml" || file_->filename == "docker-compose.yml")
      file->type = model::YamlFile::Type::DOCKER_COMPOSE;
    else if (file_->filename.find("ci") != std::string::npos)
      file->type = model::YamlFile::Type::CI;

    _ctx.db->persist(file);
  });
}

/*
 * Integration charts (root charts) may contain several
 * aliased microservices that are otherwise not contained
 * in any subcharts.
 */
void YamlParser::processIntegrationChart(model::FilePtr& file_, YAML::Node& loadedFile_)
{
  //if (!loadedFile_["dependencies"] || !loadedFile_["dependencies"].IsSequence())
  //{
    //_areDependenciesListed = false;

    model::Microservice service;
    service.file = file_->id;
    service.type = model::Microservice::ServiceType::PRODUCT;
    service.name = YAML::Dump(loadedFile_["name"]);
    service.version = YAML::Dump(loadedFile_["version"]);

    if (std::find(_processedMS.begin(), _processedMS.end(), service.name) == _processedMS.end())
    {
      service.serviceId = cc::model::createIdentifier(service);
      _ctx.db->persist(service);
      _processedMS.push_back(service.name);
    }

  //  return;
  //}

  util::OdbTransaction {_ctx.db} ([&]
  {
    for (auto iter = loadedFile_["dependencies"].begin();
         iter != loadedFile_["dependencies"].end();
         ++iter)
    {
      model::Microservice service;
      service.file = file_->id;
      service.type = model::Microservice::ServiceType::INTEGRATION;
      service.version = YAML::Dump((*iter)["version"]);

      if ((*iter)["alias"])
        service.name = YAML::Dump((*iter)["alias"]);
      else
        service.name = YAML::Dump((*iter)["name"]);

      if (std::find(_processedMS.begin(), _processedMS.end(), service.name) == _processedMS.end())
      {
        service.serviceId = cc::model::createIdentifier(service);
        _ctx.db->persist(service);
        _processedMS.push_back(service.name);
      }
    }
  });
}

void YamlParser::processRootKeys(model::FilePtr& file_, YAML::Node& loadedFile)
{
  util::OdbTransaction {_ctx.db} ([&]{
    for (const auto &pair: loadedFile)
    {
      model::YamlContentPtr content = std::make_shared<model::YamlContent>();
      content->file = file_->id;
      content->key = Dump(pair.first);
      content->value = Dump(pair.second);

      _ctx.db->persist(content);
    }
  });
}

bool YamlParser::collectAstNodes(model::FilePtr file_)
{
  // There is a possibility that there are more than 1 resource definition in one yaml file separated with `---`, we should collect them all as AstNode
  std::vector<YAML::Node> currentFile = YAML::LoadAllFromFile(file_->path);
  std::for_each(currentFile.begin(), currentFile.end(),
    [&](YAML::Node& currentFilePart)
    {
      try
      {
        processFileType(file_, currentFilePart);
        processRootKeys(file_, currentFilePart);

        for (auto it = currentFilePart.begin(); it != currentFilePart.end(); ++it)
        {
          chooseCoreNodeType(it->first, file_, model::YamlAstNode::SymbolType::Key);
          chooseCoreNodeType(it->second, file_, model::YamlAstNode::SymbolType::Value);
        }
      }catch (YAML::ParserException& e)
      {
        LOG(warning) << "[yamlparser] Exception thrown in : " << file_->path << ": " << e.what();

        util::OdbTransaction {_ctx.db} ([&]
        {
          model::BuildLog buildLog;
          buildLog.location.file = file_;
          buildLog.location.range.start.line = e.mark.line + 1;
          buildLog.location.range.start.column = e.mark.column;
          buildLog.location.range.end.line = e.mark.line + 1;
          buildLog.location.range.end.column = e.mark.column;
          buildLog.log.message = e.what();
          buildLog.log.type = model::BuildLogMessage::Error;

          _mutex.lock();
          _buildLogs.push_back(buildLog);
          _mutex.unlock();
        });

        return false;
      }
    });

  return true;
}

void YamlParser::chooseCoreNodeType(
  YAML::Node& node_,
  model::FilePtr file_,
  model::YamlAstNode::SymbolType symbolType_)
{
  switch (node_.Type())
  {
    case YAML::NodeType::Scalar:
      processAtomicNode(node_, file_, symbolType_,
        model::YamlAstNode::AstType::SCALAR);
      break;
    case YAML::NodeType::Sequence:
      processSequence(node_, file_, symbolType_);
      break;
    case YAML::NodeType::Map:
      processMap(node_, file_, symbolType_);
      break;
    case YAML::NodeType::Null:
    case YAML::NodeType::Undefined:
      break;
  }
}

void YamlParser::processAtomicNode(
  YAML::Node& node_,
  model::FilePtr file_,
  model::YamlAstNode::SymbolType symbolType_,
  model::YamlAstNode::AstType astType_)
{
  (util::OdbTransaction(_ctx.db))([&, this]
  {
    model::YamlAstNodePtr currentNode = std::make_shared<model::YamlAstNode>();
    currentNode->astValue = YAML::Dump(node_);
    currentNode->location.file = _ctx.srcMgr.getFile(file_->path);
    currentNode->location.range = getNodeLocation(node_);
    currentNode->astType = astType_;
    currentNode->symbolType = symbolType_;
    currentNode->entityHash = util::fnvHash(YAML::Dump(node_));
    currentNode->id = model::createIdentifier(*currentNode);

    _mutex.lock();
    if (!std::count_if(_astNodes.begin(), _astNodes.end(),
     [&](const auto& other){
        return currentNode->id == other->id;
      }))
    {
      _astNodes.push_back(currentNode);
    }
    _mutex.unlock();
  });
}

void YamlParser::processMap(
  YAML::Node& node_,
  model::FilePtr file_,
  model::YamlAstNode::SymbolType symbolType_)
{
  for (auto it = node_.begin(); it != node_.end(); ++it)
  {
    switch (it->first.Type())
    {
      case YAML::NodeType::Scalar:
        processAtomicNode(it->first, file_,
                          model::YamlAstNode::SymbolType::NestedKey,
                          model::YamlAstNode::AstType::MAP);
        break;
      case YAML::NodeType::Sequence:
        processSequence(it->first, file_, symbolType_);
        break;
      case YAML::NodeType::Map:
        processMap(it->first, file_, symbolType_);
        break;
      case YAML::NodeType::Null:
      case YAML::NodeType::Undefined:
        break;
    }

    switch (it->second.Type())
    {
      case YAML::NodeType::Scalar:
        processAtomicNode(it->second, file_,
          model::YamlAstNode::SymbolType::NestedValue,
          model::YamlAstNode::AstType::MAP);
        break;
      case YAML::NodeType::Sequence:
        processSequence(it->second, file_, symbolType_);
        break;
      case YAML::NodeType::Map:
        processMap(it->second, file_, symbolType_);
        break;
      case YAML::NodeType::Null:
      case YAML::NodeType::Undefined:
        break;
    }
  }
}

void YamlParser::processSequence(
  YAML::Node& node_,
  model::FilePtr file_,
  model::YamlAstNode::SymbolType symbolType_)
{
  for (size_t i = 0; i < node_.size(); ++i)
  {
    YAML::Node temp = node_[i];
    switch (temp.Type())
    {
      case YAML::NodeType::Scalar:
        processAtomicNode(temp, file_,
          model::YamlAstNode::SymbolType::Value,
          model::YamlAstNode::AstType::SEQUENCE);
        break;
      case YAML::NodeType::Sequence:
        processSequence(temp, file_, symbolType_);
        break;
      case YAML::NodeType::Map:
        processMap(temp, file_, symbolType_);
        break;
      case YAML::NodeType::Null:
      case YAML::NodeType::Undefined:
        break;
    }
  }
}

model::Range YamlParser::getNodeLocation(YAML::Node& node_)
{
  model::Range location;
  location.start.line = node_.Mark().line + 1;
  location.start.column = node_.Mark().column;
  std::string nodeValue = YAML::Dump(node_);
  auto count = std::count(nodeValue.begin(), nodeValue.end(), '\n');
  location.end.line = node_.Mark().line + count + 1;

  if (count > 0)
  {
    auto pos = nodeValue.find_last_of('\n');
    location.end.column = nodeValue.size() - pos - 1;
  }
  else
  {
    location.end.column = node_.Mark().column + nodeValue.size() + 1;
  }

  return location;
}

YamlParser::~YamlParser()
{
  (util::OdbTransaction(_ctx.db))([this]{
     util::persistAll(_astNodes, _ctx.db);

     for (model::BuildLog& log : _buildLogs)
       _ctx.db->persist(log);
  });
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreturn-type-c-linkage"
extern "C"
{
boost::program_options::options_description getOptions()
{
  boost::program_options::options_description description("Yaml Plugin");
  return description;
}

std::shared_ptr<YamlParser> make(ParserContext& ctx_)
{
  return std::make_shared<YamlParser>(ctx_);
}
}
#pragma clang diagnostic pop

}
}
