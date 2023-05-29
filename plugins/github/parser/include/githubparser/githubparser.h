#ifndef CC_PARSER_GITHUBPARSER_H
#define CC_PARSER_GITHUBPARSER_H

#include <list>

#include <parser/abstractparser.h>
#include <parser/parsercontext.h>

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/property_tree/ptree.hpp>

#include <certify/extensions.hpp>
#include <certify/https_verification.hpp>

#include "githubdataconverter.h"

namespace cc
{
namespace parser
{
namespace beast = boost::beast;
namespace asio = boost::asio;
namespace ssl = asio::ssl;
namespace http = boost::beast::http;
using tcp = boost::asio::ip::tcp;
namespace pt = boost::property_tree;

typedef tcp::resolver::results_type ResType;
typedef ssl::stream<tcp::socket> SSLStream;
typedef tcp::socket Socket;
typedef http::response<http::string_body> HTTPResponse;

class GitHubParser : public AbstractParser
{
public:
  GitHubParser(ParserContext& ctx_);
  virtual ~GitHubParser();
  bool parse() override;

private:
  GitHubDataConverter _converter = GitHubDataConverter(_ctx);
  static const std::list<std::string> uriList;
  std::string _owner;
  std::string _repoName;
  std::string _authString;

  bool accept(
    const std::string& path_);
  std::string encode64(
    const std::string& string);

  ResType resolve(
    asio::io_context& ctx,
    std::string const& hostname);
  Socket connect(
    asio::io_context& ctx,
    std::string const& hostname);
  std::unique_ptr<SSLStream> connect(
    asio::io_context& ctx,
    ssl::context& ssl_ctx,
    std::string const& hostname);
  HTTPResponse get(
    ssl::stream<tcp::socket>& stream,
    boost::string_view hostname,
    boost::string_view uri);

  void processUrl(
    std::string url_);
  std::string createUri(
    std::string const& ending_);
  pt::ptree createPTree(
    asio::io_context& ctx,
    ssl::context& ssl_ctx,
    std::string const& hostname,
    std::string const& uri);
  void processLabels(
    pt::ptree& ptree);
  void processMilestones(
    pt::ptree& ptree);
  void processContributors(
    pt::ptree& ptree,
    asio::io_context& ctx,
    ssl::context& ssl_ctx,
    const std::string& hostname);
  void processCommits(
    pt::ptree& ptree,
    asio::io_context& ctx,
    ssl::context& ssl_ctx,
    const std::string& hostname,
    const std::string& uri);
  void processIssues(
    pt::ptree& ptree,
    asio::io_context& ctx,
    ssl::context& ssl_ctx,
    const std::string& hostname);
  void processPulls(
    pt::ptree& ptree,
    asio::io_context& ctx,
    ssl::context& ssl_ctx,
    const std::string& hostname,
    const std::string& uri);
  void processNewUsers(
    pt::ptree& ptree,
    asio::io_context& ctx,
    ssl::context& ssl_ctx,
    const std::string& hostname);
  void runGitHubParser();

};
  
} // parser
} // cc

#endif // CC_PARSER_GITHUBPARSER_H
