#ifndef CC_SERVICE_GITHUB_GITHUBSERVICE_H
#define CC_SERVICE_GITHUB_GITHUBSERVICE_H

#include <memory>
#include <vector>
#include <set>

#include <boost/program_options/variables_map.hpp>

#include <odb/database.hxx>

#include <model/person.h>
#include <model/person-odb.hxx>
#include <model/pull.h>
#include <model/pull-odb.hxx>
#include <model/issue.h>
#include <model/issue-odb.hxx>

#include <util/odbtransaction.h>
#include <webserver/servercontext.h>

#include "GitHubService.h"

namespace cc
{
namespace service
{
namespace github
{

class GitHubServiceHandler : virtual public GitHubServiceIf
{
public:
  GitHubServiceHandler(
    std::shared_ptr<odb::database> db_,
    std::shared_ptr<std::string> datadir_,
    const cc::webserver::ServerContext& context_);

  void getPull(
    Pull& return_,
    const std::int64_t pullNum_) override;
  void getPullList(
    std::vector<Pull>& return_) override;
  void getContributorList(
    std::vector<Person>& return_) override;

  void getFileListForPull(
    std::vector<PullFile>& return_,
    const std::int64_t pullNum_) override;
  void getLabelListForPull(
    std::vector<Label>& return_,
    const std::int64_t pullNum_) override;
  void getReviewerListForPull(
    std::vector<Person>& return_,
    const std::int64_t pullNum_) override;

  void getContributionsForPull(
    std::vector<Contribution>& return_,
    const std::int64_t pullNum_) override;
  void getContributionsForPullByAuthor(
    Contribution& return_,
    const std::int64_t pullNum_,
    const std::string& username_) override;

  void getCommentsForPullFile(
    std::vector<Comment>& return_,
    const std::int64_t pullNum_,
    const std::string& path_) override;
  std::int64_t getCommentCountForPullFile(
    const std::int64_t pullNum_,
    const std::string& path_) override;
  void getCommentsForPullFilePerDiffHunk (
    std::vector<CommentsByDiffHunk>& return_,
    const std::int64_t pullNum_,
    const std::string& path_) override;
  void getCommentCountForPullFilePerAuthor (
    std::vector<CommentCountPerAuthor>& return_,
    const std::int64_t pullNum_,
    const std::string& path_) override;

  void getPullListForAuthor(
    std::vector<Pull>& return_,
    const std::string& user_) override;
  void getPullListForReviewer(
    std::vector<Pull>& return_,
    const std::string& user_) override;

  void getGitHubString(std::string& str_);

private:
  std::shared_ptr<odb::database> _db;
  util::OdbTransaction _transaction;

  const boost::program_options::variables_map& _config;
};

} // github
} // service
} // cc

#endif // CC_SERVICE_GITHUB_GITHUBSSERVICE_H
