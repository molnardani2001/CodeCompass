#include <githubparser/githubparser.h>

#include <boost/filesystem.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/transform_width.hpp>

#include "githubdataconverter.h"

#include <model/label.h>
#include <model/label-odb.hxx>
#include <model/milestone.h>
#include <model/milestone-odb.hxx>
#include <model/person.h>
#include <model/person-odb.hxx>
#include <model/commit.h>
#include <model/commit-odb.hxx>
#include <model/commitfile.h>
#include <model/commitfile-odb.hxx>
#include <model/issue.h>
#include <model/issue-odb.hxx>
#include <model/pull.h>
#include <model/pull-odb.hxx>
#include <model/pullfile.h>
#include <model/pullfile-odb.hxx>
#include <model/review.h>
#include <model/review-odb.hxx>
#include <model/comment.h>
#include <model/comment-odb.hxx>

#include <util/logutil.h>
#include <util/odbtransaction.h>

#include <memory>

namespace cc
{
namespace parser
{

const std::list<std::string> GitHubParser::uriList ({"labels", "milestones", "contributors",
                                                     "commits", "issues", "pulls"});

std::string GitHubParser::encode64(const std::string &val) {
  using namespace boost::archive::iterators;
  using It = base64_from_binary<transform_width<std::string::const_iterator, 6, 8>>;
  auto tmp = std::string(It(std::begin(val)), It(std::end(val)));
  return tmp.append((3 - val.size() % 3) % 3, '=');
}

ResType GitHubParser::resolve(
  asio::io_context& ctx,
  std::string const& hostname)
{
  tcp::resolver resolver{ctx};
  return resolver.resolve(hostname, "https");
}

Socket GitHubParser::connect(
  asio::io_context& ctx,
  std::string const& hostname)
{
  tcp::socket socket{ctx};
  asio::connect(socket, resolve(ctx, hostname));
  return socket;
}

std::unique_ptr<SSLStream> GitHubParser::connect(
  asio::io_context& ctx,
  ssl::context& ssl_ctx,
  std::string const& hostname)
{
  auto stream = boost::make_unique<ssl::stream<tcp::socket>>(connect(ctx, hostname), ssl_ctx);
  // tag::stream_setup_source[]
  boost::certify::set_server_hostname(*stream, hostname);
  boost::certify::sni_hostname(*stream, hostname);
  // end::stream_setup_source[]

  stream->handshake(ssl::stream_base::handshake_type::client);
  return stream;
}

HTTPResponse GitHubParser::get(
  ssl::stream<tcp::socket>& stream,
  boost::string_view hostname,
  boost::string_view uri)
{
  http::request<http::empty_body> request;
  request.method(http::verb::get);
  request.target(uri);
  request.keep_alive(false);
  request.set(http::field::host, hostname);
  request.set(http::field::user_agent, "CodeCompass");
  request.set(http::field::authorization, _authString);
  http::write(stream, request);

  http::response<http::string_body> response;
  beast::flat_buffer buffer;
  http::read(stream, buffer, response);

  return response;
}

void GitHubParser::processUrl(std::string url_)
{
  url_.erase(0, url_.find("github"));
  url_.erase(0, url_.find('/') + 1);
  _owner = url_.substr(0, url_.find('/'));
  url_.erase(0, url_.find('/') + 1);
  _repoName = url_.substr(0, url_.find('/'));
}

std::string GitHubParser::createUri(std::string const& ending_)
{
  return "/repos/" + _owner + "/" + _repoName + "/" + ending_;
}

pt::ptree GitHubParser::createPTree(
  asio::io_context &ctx,
  ssl::context &ssl_ctx,
  const std::string &hostname,
  const std::string &uri)
{
  pt::ptree ptree;

  auto stream_ptr = connect(ctx, ssl_ctx, hostname);
  auto response = get(*stream_ptr, hostname, uri);
  boost::system::error_code ec;
  stream_ptr->shutdown(ec);

  if (response.body() == "[]") return ptree;

  std::stringstream ss;
  ss << response.body();
  pt::read_json(ss, ptree);
  return ptree;
}

void GitHubParser::processNewUsers(
  pt::ptree& ptree,
  asio::io_context& ctx,
  ssl::context& ssl_ctx,
  const std::string& hostname)
{
  GitHubDataConverter converter(_ctx);
  std::set<std::string> newUsers = converter.GetNewUsers(ptree);
  for (auto user : newUsers)
  {
    pt::ptree userPtree= createPTree(ctx, ssl_ctx, hostname, "/users/" + user);
    util::OdbTransaction trans(_ctx.db);
    trans([&, this]{
      _ctx.db->persist(converter.ConvertUser(userPtree, 0));
    });
  }
}

void GitHubParser::runClient()
{
  GitHubDataConverter converter(_ctx);
  util::OdbTransaction trans(_ctx.db);

  asio::io_context ctx;
  ssl::context ssl_ctx{ssl::context::tls_client};
  auto const hostname = std::string{"api.github.com"};
  ssl_ctx.set_verify_mode(ssl::context::verify_peer |
                          ssl::context::verify_fail_if_no_peer_cert);
  ssl_ctx.set_default_verify_paths();
  boost::certify::enable_native_https_server_verification(ssl_ctx);

  for (const auto & it : uriList)
  {
    bool lastPageReached = false;
    int pageNum = 1;
    while(!lastPageReached)
    {
      LOG(info) << "Processing page " << pageNum << " of " << it << ".";
      pt::ptree ptree;

      if (it == "issues" || it == "pulls" || it == "milestones")
      {
        ptree = createPTree(ctx, ssl_ctx, hostname, createUri(it + "?per_page=100&state=all&page=" + std::to_string(pageNum)));
      }
      else
      {
        ptree = createPTree(ctx, ssl_ctx, hostname, createUri(it + "?per_page=100&page=" + std::to_string(pageNum)));
      }

      lastPageReached = (ptree.empty());
      if (lastPageReached) continue;

      if (it == "labels")
      {
        std::vector<model::Label> labels = converter.ConvertLabels(ptree);
        for (auto label : labels)
        {
          trans([&, this]{
            _ctx.db->persist(label);
          });
        }
      }
      else if (it == "milestones")
      {
        std::vector<model::Milestone> milestones = converter.ConvertMilestones(ptree);
        for (auto milestone : milestones)
        {
          trans([&, this]{
            _ctx.db->persist(milestone);
          });
        }
      }
      else if (it == "contributors")
      {
        for (auto pair : converter.ConvertContributors(ptree))
        {
          pt::ptree userPtree = createPTree(ctx, ssl_ctx, hostname, "/users/" + pair.first);
          trans([&, this]{
              _ctx.db->persist(converter.ConvertUser(userPtree, pair.second));
            });
        }
      }
      else if (it == "commits")
      {
        std::vector<model::Commit> commits = converter.ConvertCommits(ptree);
        for (auto commit : commits)
        {
          pt::ptree commitPtree = createPTree(ctx, ssl_ctx, hostname, createUri(it + "/" + commit.sha));
          std::vector<model::CommitFile> commitFiles = converter.ConvertCommitFiles(commitPtree, commit);
          for (auto commitFile : commitFiles)
          {
            trans([&, this]{
              _ctx.db->persist(commitFile);
              commit.commitFiles.emplace_back(std::make_shared<model::CommitFile>(commitFile));
            });
          }
          trans([&, this]{
            _ctx.db->persist(commit);
          });
        }
      }
      else if (it == "issues")
      {
        processNewUsers(ptree, ctx, ssl_ctx, hostname);

        std::vector<model::Issue> issues = converter.ConvertIssues(ptree);
        for (auto issue : issues)
        {
          trans([&, this]{
            _ctx.db->persist(issue);
          });
        }
      }
      else if (it == "pulls")
      {
        processNewUsers(ptree, ctx, ssl_ctx, hostname);

        std::vector<model::Pull> pulls = converter.ConvertPulls(ptree);
        for (auto pull : pulls)
        {
          int tempPageNum = 1;
          pt::ptree pullFilePtree = createPTree(ctx, ssl_ctx, hostname, createUri(
            it + "/" + std::to_string(pull.number) + "/files?per_page=100&page=" + std::to_string(tempPageNum)));
          while(!pullFilePtree.empty())
          {
            std::vector<model::PullFile> pullFiles = converter.ConvertPullFiles(pullFilePtree, pull);
            for (auto pullFile: pullFiles)
            {
              trans([&, this]
                    {
                      _ctx.db->persist(pullFile);
                      pull.pullFiles.emplace_back(std::make_shared<model::PullFile>(pullFile));
                    });
            }

            tempPageNum++;
            pullFilePtree = createPTree(ctx, ssl_ctx, hostname, createUri(
              it + "/" + std::to_string(pull.number) + "/files?per_page=100&page=" + std::to_string(tempPageNum)));
          }

          tempPageNum = 1;
          pt::ptree reviewPtree = createPTree(ctx, ssl_ctx, hostname, createUri(
            it + "/" + std::to_string(pull.number) + "/reviews?per_page=100&page=" + std::to_string(tempPageNum)));
          while(!reviewPtree.empty())
          {
            processNewUsers(reviewPtree, ctx, ssl_ctx, hostname);

            std::vector<model::Review> reviews = converter.ConvertPullReviews(reviewPtree, pull);
            for (const auto& review : reviews)
            {
              trans([&, this]{
                _ctx.db->persist(review);
              });
            }

            tempPageNum++;
            reviewPtree = createPTree(ctx, ssl_ctx, hostname, createUri(
              it + "/" + std::to_string(pull.number) + "/reviews?per_page=100&page=" + std::to_string(tempPageNum)));
          }

          tempPageNum = 1;
          pt::ptree commentPtree = createPTree(ctx, ssl_ctx, hostname, createUri(
            it + "/" + std::to_string(pull.number) + "/comments?per_page=100&page=" + std::to_string(tempPageNum)));
          while(!commentPtree.empty())
          {
            processNewUsers(commentPtree, ctx, ssl_ctx, hostname);

            std::vector<model::Comment> comments = converter.ConvertPullComments(commentPtree, pull);
            for (auto comment : comments)
            {
              trans([&, this]{
                _ctx.db->persist(comment);
              });
            }

            tempPageNum++;
            commentPtree = createPTree(ctx, ssl_ctx, hostname, createUri(
              it + "/" + std::to_string(pull.number) + "/comments?per_page=100&page=" + std::to_string(tempPageNum)));
          }

          pt::ptree pullPtree = createPTree(ctx, ssl_ctx, hostname, createUri(it + "/" + std::to_string(pull.number)));
          trans([&, this]{
            _ctx.db->persist(converter.ConvertPull(pullPtree, pull));
          });
        }
      }
      else
      {
        lastPageReached = true;
      }
      pageNum++;
    }
  }

}

GitHubParser::GitHubParser(ParserContext& ctx_): AbstractParser(ctx_)
{
}

bool GitHubParser::accept(const std::string& path_)
{
  std::string ext = boost::filesystem::extension(path_);
  return ext == ".github";
}

bool GitHubParser::parse()
{
  for(std::string path : _ctx.options["input"].as<std::vector<std::string>>())
  {
    if(accept(path))
    {
      LOG(info) << "GitHubParser parse path: " << path;
    }
  }

  if(!_ctx.options.count("repo-url"))
  {
    LOG(error) << "Missing repository URL. Cannot run GitHub parser.";
    return false;
  }
  processUrl(_ctx.options["repo-url"].as<std::string>());

  _authString = "Basic " +  encode64("-u " + _ctx.options["github-user"].as<std::string>() +
           ":" +  _ctx.options["github-token"].as<std::string>());
  //LOG(debug) << _authString;

  runClient();

  return true;
}

  GitHubParser::~GitHubParser()
{
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreturn-type-c-linkage"
extern "C"
{
  boost::program_options::options_description getOptions()
  {
    boost::program_options::options_description description("GitHub Plugin");

    description.add_options()
        ("github-user", po::value<std::string>(),
          "Username for authentication to GitHub")
        ("github-token", po::value<std::string>(),
         "Token for authentication to GitHub")
        ("repo-url", po::value<std::string>(),
          "URL of the parsed repository.");

    return description;
  }

  std::shared_ptr<GitHubParser> make(ParserContext& ctx_)
  {
    return std::make_shared<GitHubParser>(ctx_);
  }
}
#pragma clang diagnostic pop

} // parser
} // cc
