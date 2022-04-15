#include <service/githubservice.h>
#include <util/dbutil.h>
#include <odb/database.hxx>
#include <odb/result.hxx>
#include <vector>

namespace cc
{
namespace service
{
namespace github
{

GitHubServiceHandler::GitHubServiceHandler(
  std::shared_ptr<odb::database> db_,
  std::shared_ptr<std::string> /*datadir_*/,
  const cc::webserver::ServerContext& context_)
    : _db(db_), _transaction(db_), _config(context_.options)
{
}

void GitHubServiceHandler::getPullList(
  std::vector<Pull>& return_)
{
  _transaction([&, this]() {
    odb::result<model::Pull> res (_db->query<model::Pull>());
    for(auto iter = res.begin(); iter != res.end(); ++iter)
    {
      iter->user.load();
      iter->mergedBy.load();
      Pull p;
      p.number = iter->number;
      p.title = iter->title;
      p.body = iter->body;
      p.user = iter->user->username;
      p.createdAt = iter->createdAt;
      p.headLabel = iter->headLabel;
      p.baseLabel = iter->baseLabel;
      p.isOpen = iter->isOpen;
      p.isMerged = iter->isMerged;
      p.mergedBy = (iter->mergedBy == nullptr) ? "" : iter->mergedBy->username;
      return_.emplace_back(p);
    }
  });
}

void GitHubServiceHandler::getFileListForPull(
  std::vector<PullFile>& return_,
  const std::int64_t pullNum_)
{
  _transaction([&, this]() {
    odb::result<model::PullFile> res (_db->query<model::PullFile>(
      odb::query<model::PullFile>::prNumber == pullNum_));
    for(auto iter = res.begin(); iter != res.end(); ++iter)
    {
      PullFile f;
      f.sha = iter->sha;
      f.path = iter->path;
      return_.emplace_back(f);
    }
  });
}

void GitHubServiceHandler::getLabelListForPull(
  std::vector<Label>& return_,
  const std::int64_t pullNum_)
{
  _transaction([&, this]() {
    std::shared_ptr<model::Pull> pull =  (_db->query_one<model::Pull>(
      odb::query<model::Pull>::number == pullNum_));
    for(auto iter = pull->labels.begin(); iter != pull->labels.end(); ++iter)
    {
      Label l;
      iter->load();
      l.id = iter->get()->id;
      l.name = iter->get()->name;
      return_.emplace_back(l);
      LOG(info) << l.name;
    }
  });
}

void GitHubServiceHandler::getReviewerListForPull(
  std::vector<Person>& return_,
  const std::int64_t pullNum_)
{
  _transaction([&, this]() {
    std::shared_ptr<model::Pull> pull =  (_db->query_one<model::Pull>(
      odb::query<model::Pull>::number == pullNum_));
    for(auto iter = pull->reviewers.begin(); iter != pull->reviewers.end(); ++iter)
    {
      Person p;
      p.username = iter->get()->username;
      p.name = (iter->get()->name == "null") ? "" : iter->get()->name;
      p.company = (iter->get()->company == "null") ? "" : iter->get()->company;
      p.url = iter->get()->url;
      return_.emplace_back(p);
    }
  });
}

void GitHubServiceHandler::getContributorList(
  std::vector<Person>& return_)
{
  _transaction([&, this]() {
    odb::result<model::Person> res (_db->query<model::Person>());
    for(auto iter = res.begin(); iter != res.end(); ++iter)
    {
      Person p;
      p.username = iter->username;
      p.name = (iter->name == "null") ? "" : iter->name;
      p.company = (iter->company == "null") ? "" : iter->company;
      p.url = iter->url;
      return_.emplace_back(p);
    }
  });
}

void GitHubServiceHandler::getPullListForAuthor(
  std::vector<Pull>& return_,
  const std::string& user)
{
  _transaction([&, this]() {
    odb::result<model::Pull> res (_db->query<model::Pull>(
      odb::query<model::Pull>::user->username == user));
    for(auto iter = res.begin(); iter != res.end(); ++iter)
    {
      iter->user.load();
      iter->mergedBy.load();
      Pull p;
      p.number = iter->number;
      p.title = iter->title;
      p.body = iter->body;
      p.user = iter->user->username;
      p.createdAt = iter->createdAt;
      p.headLabel = iter->headLabel;
      p.baseLabel = iter->baseLabel;
      p.isOpen = iter->isOpen;
      p.isMerged = iter->isMerged;
      p.mergedBy = (iter->mergedBy == nullptr) ? "" : iter->mergedBy->username;
      return_.emplace_back(p);
    }
  });
}

//ez most az összes pullt listázza
void GitHubServiceHandler::getPullListForReviewer(
  std::vector<Pull>& return_,
  const std::string& user)
{
  _transaction([&, this](){
    typedef odb::query<model::Pull> PullQuery;
    odb::result<model::Pull> res (_db->query<model::Pull>(
      PullQuery::user == user));
    for(auto iter = res.begin(); iter != res.end(); ++iter)
    {
      iter->user.load();
      iter->mergedBy.load();
      Pull p;
      p.number = iter->number;
      p.title = iter->title;
      p.body = iter->body;
      p.user = iter->user->username;
      p.createdAt = iter->createdAt;
      p.headLabel = iter->headLabel;
      p.baseLabel = iter->baseLabel;
      p.isOpen = iter->isOpen;
      p.isMerged = iter->isMerged;
      p.mergedBy = (iter->mergedBy == nullptr) ? "" : iter->mergedBy->username;
      return_.emplace_back(p);
    }
  });
  for (const auto& var : return_)
    LOG(info) << var.number << "   " << var.user;
}

/*void GitHubServiceHandler::getIssueList(
  std::vector<model::Issue>& return_)
{
  _transaction([&, this]() {
    odb::result<model::Issue> res (_db->query<model::Issue>());

    return_.insert(return_.begin(), res.begin(), res.end());
  });
}*/

void GitHubServiceHandler::getGitHubString(std::string& str_)
{
  str_ = "test";
  //str_ = _config["github-result"].as<std::string>();
}

} // github
} // service
} // cc
