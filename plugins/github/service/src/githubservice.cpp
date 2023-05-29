#include <service/githubservice.h>
#include <util/dbutil.h>
#include <odb/database.hxx>
#include <odb/result.hxx>
#include <vector>
#include <set>
#include <map>
#include <tuple>
#include <algorithm>

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

void GitHubServiceHandler::getPull(
    Pull& return_,
    const std::int64_t pullNum_)
{
  _transaction([&, this]() {
    std::shared_ptr<model::Pull> pull =  (_db->query_one<model::Pull>(
      odb::query<model::Pull>::number == pullNum_));

    if (!pull) return;

    pull->user.load();
    pull->mergedBy.load();
    Pull p;
    p.number = pull->number;
    p.title = pull->title;
    p.body = pull->body;
    p.user = pull->user->username;
    p.createdAt = pull->createdAt;
    p.updatedAt = pull->updatedAt;
    p.headLabel = pull->headLabel;
    p.baseLabel = pull->baseLabel;
    p.isOpen = pull->isOpen;
    p.isMerged = pull->isMerged;
    p.mergedBy = (pull->mergedBy == nullptr) ? "" : pull->mergedBy->username;
    p.closedAt = pull->closedAt;
    p.mergedAt = pull->mergedAt;
    p.additions = pull->additions;
    p.deletions = pull->deletions;
    p.changedFiles = pull->changedFiles;
    return_ = p;
  });
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
      p.updatedAt = iter->updatedAt;
      p.headLabel = iter->headLabel;
      p.baseLabel = iter->baseLabel;
      p.isOpen = iter->isOpen;
      p.isMerged = iter->isMerged;
      p.mergedBy = (iter->mergedBy == nullptr) ? "" : iter->mergedBy->username;
      p.closedAt = iter->closedAt;
      p.mergedAt = iter->mergedAt;
      p.additions = iter->additions;
      p.deletions = iter->deletions;
      p.changedFiles = iter->changedFiles;
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
      f.status = iter->status;
      f.prNumber = iter->prNumber;
      f.additions = iter->additions;
      f.deletions = iter->deletions;
      f.changes = iter->changes;
      f.patch = iter->patch;
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
    std::set<std::string> reviewers;
    for(auto iter = pull->reviewers.begin(); iter != pull->reviewers.end(); ++iter)
    {
      Person p;
      p.username = iter->get()->username;
      p.name = (iter->get()->name == "null") ? "" : iter->get()->name;
      p.company = (iter->get()->company == "null") ? "" : iter->get()->company;
      p.url = iter->get()->url;

      if (reviewers.insert(p.username).second)
      {
        return_.emplace_back(p);
      }
    }
  });
}

void GitHubServiceHandler::getContributionsForPull(
  std::vector<Contribution> &return_,
  const std::int64_t pullNum_)
{
  _transaction([&, this]() {
    std::shared_ptr<model::Pull> pull =  (_db->query_one<model::Pull>(
      odb::query<model::Pull>::number == pullNum_));
    std::map<std::string, std::tuple<std::int32_t, std::int32_t, std::int32_t>> m;
    for(auto iter = pull->reviews.begin(); iter != pull->reviews.end(); ++iter)
    {
      iter->load();
      iter->get()->user.load();

      auto item = m.find(iter->get()->user->username);
      if(item != m.end())
      {
        if (iter->get()->state == "COMMENTED") std::get<0>(item->second)++;
        else if (iter->get()->state == "CHANGES_REQUESTED") std::get<1>(item->second)++;
        else if (iter->get()->state == "APPROVED") std::get<2>(item->second)++;
      }
      else
      {
        std::pair<std::string, std::tuple<std::int32_t, std::int32_t, std::int32_t>> p ({iter->get()->user->username,{0, 0, 0}});
        if (iter->get()->state == "COMMENTED") std::get<0>(p.second)++;
        else if (iter->get()->state == "CHANGES_REQUESTED") std::get<1>(p.second)++;
        else if (iter->get()->state == "APPROVED") std::get<2>(p.second)++;
        m.insert(p);
      }
    }
    for (auto item : m)
    {
      Contribution c;
      c.user = item.first;
      c.comments = std::get<0>(item.second);
      c.changeRequests = std::get<1>(item.second);
      c.approves = std::get<2>(item.second);
      return_.emplace_back(c);
    }
  });
}

void GitHubServiceHandler::getContributionsForPullByAuthor(
  Contribution& return_,
  const std::int64_t pullNum_,
  const std::string& username_)
{
  _transaction([&, this]() {
    std::shared_ptr<model::Pull> pull =  (_db->query_one<model::Pull>(
      odb::query<model::Pull>::number == pullNum_));
    std::tuple<std::int32_t, std::int32_t, std::int32_t> t;
    for(auto iter = pull->reviews.begin(); iter != pull->reviews.end(); ++iter)
    {
      iter->load();
      iter->get()->user.load();

      if (username_ == iter->get()->user->username)
      {
        if (iter->get()->state == "COMMENTED") std::get<0>(t)++;
        else if (iter->get()->state == "CHANGES_REQUESTED") std::get<1>(t)++;
        else if (iter->get()->state == "APPROVED") std::get<2>(t)++;
      }
    }

    return_.user = username_;
    return_.comments = std::get<0>(t);
    return_.changeRequests = std::get<1>(t);
    return_.approves = std::get<2>(t);
  });
}

std::int64_t GitHubServiceHandler::getCommentCountForPullFile(
  const std::int64_t pullNum_,
  const std::string& path_)
{
  std::int64_t ret = 0;
  _transaction([&, this]() {
    std::shared_ptr<model::Pull> pull =  (_db->query_one<model::Pull>(
      odb::query<model::Pull>::number == pullNum_));
    for(auto iter = pull->reviewComments.begin(); iter != pull->reviewComments.end(); ++iter)
    {
      iter->load();

      std::string temp_path = path_;
      std::size_t index = temp_path.find_last_of("/\\");
      auto it = std::next(temp_path.begin(), index);
      std::replace(it, temp_path.end(), '-', '_');

      if (temp_path == iter->get()->path)
      {
        ret++;
      }
    }
  });
  return ret;
}

void GitHubServiceHandler::getCommentsForPullFilePerDiffHunk (
  std::vector<CommentsByDiffHunk>& return_,
  const std::int64_t pullNum_,
  const std::string& path_)
{
  _transaction([&, this]() {
    std::shared_ptr<model::Pull> pull =  (_db->query_one<model::Pull>(
      odb::query<model::Pull>::number == pullNum_));
    std::map<std::string, std::vector<Comment>> m;
    for(auto iter = pull->reviewComments.begin(); iter != pull->reviewComments.end(); ++iter)
    {
      iter->load();

      std::string temp_path = path_;
      std::size_t index = temp_path.find_last_of("/\\");
      auto it = std::next(temp_path.begin(), index);
      std::replace(it, temp_path.end(), '-', '_');

      if (temp_path == iter->get()->path)
      {
        iter->get()->user.load();

        Comment c;
        c.id = iter->get()->id;
        c.pullReviewId = iter->get()->pullReviewId;
        c.commitId = iter->get()->commitId;
        c.originalCommitId = iter->get()->originalCommitId;
        c.user = iter->get()->user->username;
        c.diffHunk = iter->get()->diffHunk;
        c.path = iter->get()->path;
        c.body = iter->get()->body;
        c.url = iter->get()->url;
        c.createdAt = iter->get()->createdAt;
        c.updatedAt = iter->get()->updatedAt;

        auto item = m.find(iter->get()->diffHunk);
        if(item != m.end())
        {
          item->second.emplace_back(c);
        }
        else
        {
          std::pair<std::string, std::vector<Comment>> p;
          p.first = iter->get()->diffHunk;
          p.second.emplace_back(c);
          m.insert(p);
        }
      }
    }
    for (auto item : m)
    {
      CommentsByDiffHunk c;
      c.key = item.first;
      c.value = item.second;
      return_.emplace_back(c);
    }
  });
}

void GitHubServiceHandler::getCommentCountForPullFilePerAuthor (
  std::vector<CommentCountPerAuthor>& return_,
  const std::int64_t pullNum_,
  const std::string& path_)
{
  _transaction([&, this]() {
    std::shared_ptr<model::Pull> pull =  (_db->query_one<model::Pull>(
      odb::query<model::Pull>::number == pullNum_));
    std::map<std::string, std::int64_t> m;
    for(auto iter = pull->reviewComments.begin(); iter != pull->reviewComments.end(); ++iter)
    {
      iter->load();

      std::string temp_path = path_;
      std::size_t index = temp_path.find_last_of("/\\");
      auto it = std::next(temp_path.begin(), index);
      std::replace(it, temp_path.end(), '-', '_');

      if (temp_path == iter->get()->path)
      {
        iter->get()->user.load();
        auto item = m.find(iter->get()->user->username);
        if(item != m.end())
        {
          item->second++;
        }
        else
        {
          std::pair<std::string, std::int64_t> p({iter->get()->user->username, 1});
          m.insert(p);
        }
      }
    }
    for (auto item : m)
    {
      CommentCountPerAuthor c;
      c.key = item.first;
      c.value = item.second;
      return_.emplace_back(c);
    }
  });
}

void GitHubServiceHandler::getCommentsForPullFile(
  std::vector<Comment>& return_,
  const std::int64_t pullNum_,
  const std::string& path_)
{
  _transaction([&, this]() {
    std::shared_ptr<model::Pull> pull =  (_db->query_one<model::Pull>(
      odb::query<model::Pull>::number == pullNum_));
    for(auto iter = pull->reviewComments.begin(); iter != pull->reviewComments.end(); ++iter)
    {
      iter->load();

      std::string temp_path = path_;
      std::size_t index = temp_path.find_last_of("/\\");
      auto it = std::next(temp_path.begin(), index);
      std::replace(it, temp_path.end(), '-', '_');

      if (temp_path == iter->get()->path)
      {
        iter->get()->user.load();

        Comment c;
        c.id = iter->get()->id;
        c.pullReviewId = iter->get()->pullReviewId;
        c.commitId = iter->get()->commitId;
        c.originalCommitId = iter->get()->originalCommitId;
        c.user = iter->get()->user->username;
        c.diffHunk = iter->get()->diffHunk;
        c.path = iter->get()->path;
        c.body = iter->get()->body;
        c.url = iter->get()->url;
        c.createdAt = iter->get()->createdAt;
        c.updatedAt = iter->get()->updatedAt;
        return_.emplace_back(c);
      }
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
  const std::string& user_)
{
  _transaction([&, this]() {
    odb::result<model::Pull> res (_db->query<model::Pull>(
      odb::query<model::Pull>::user->username == user_));
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
      p.updatedAt = iter->updatedAt;
      p.headLabel = iter->headLabel;
      p.baseLabel = iter->baseLabel;
      p.isOpen = iter->isOpen;
      p.isMerged = iter->isMerged;
      p.mergedBy = (iter->mergedBy == nullptr) ? "" : iter->mergedBy->username;
      p.closedAt = iter->closedAt;
      p.mergedAt = iter->mergedAt;
      p.additions = iter->additions;
      p.deletions = iter->deletions;
      p.changedFiles = iter->changedFiles;
      return_.emplace_back(p);
    }
  });
}

void GitHubServiceHandler::getPullListForReviewer(
  std::vector<Pull>& return_,
  const std::string& user_)
{
  _transaction([&, this](){
    odb::result<model::Pull> res (_db->query<model::Pull>());
    std::set<std::string> pulls;
    for(auto iter = res.begin(); iter != res.end(); ++iter)
    {
      for (auto reviewer = iter->reviewers.begin(); reviewer != iter->reviewers.end(); ++reviewer)
      {
        if (reviewer->get()->username == user_)
        {
          Pull p;
          p.title = iter->title;

          if (pulls.insert(p.title).second)
          {
            iter->user.load();
            iter->mergedBy.load();

            p.number = iter->number;
            p.body = iter->body;
            p.user = iter->user->username;
            p.createdAt = iter->createdAt;
            p.updatedAt = iter->updatedAt;
            p.headLabel = iter->headLabel;
            p.baseLabel = iter->baseLabel;
            p.isOpen = iter->isOpen;
            p.isMerged = iter->isMerged;
            p.mergedBy = (iter->mergedBy == nullptr) ? "" : iter->mergedBy->username;
            p.closedAt = iter->closedAt;
            p.mergedAt = iter->mergedAt;
            p.additions = iter->additions;
            p.deletions = iter->deletions;
            p.changedFiles = iter->changedFiles;
            return_.emplace_back(p);
          }
        }
      }
    }
  });
}

void GitHubServiceHandler::getGitHubString(std::string& str_)
{
  str_ = _config["github-result"].as<std::string>();
}

} // github
} // service
} // cc
