#include "githubdataconverter.h"

#include <utility>
#include <util/logutil.h>
#include <util/odbtransaction.h>
#include <odb/session.hxx>

#include <model/milestone-odb.hxx>
#include <model/label-odb.hxx>
#include <model/pull-odb.hxx>
#include <model/person-odb.hxx>

namespace cc
{
namespace parser
{
GitHubDataConverter::GitHubDataConverter(ParserContext& ctx) : _ctx(ctx) {}

std::vector<model::Label> GitHubDataConverter::ConvertLabels(pt::ptree& ptree)
{
  std::vector<model::Label> labels;
  for (pt::ptree::value_type &labelData : ptree)
  {
    model::Label label;

    label.id = labelData.second.get<std::uint64_t>("id");
    label.name = labelData.second.get<std::string>("name");
    label.description = labelData.second.get<std::string>("description");

    labels.push_back(label);
  }
  return labels;
}

std::vector<model::Milestone> GitHubDataConverter::ConvertMilestones(pt::ptree& ptree)
{
  std::vector<model::Milestone> milestones;
  for (pt::ptree::value_type &milestoneData : ptree)
  {
    model::Milestone milestone;

    milestone.number = milestoneData.second.get<unsigned>("number");
    milestone.title = milestoneData.second.get<std::string>("title");
    milestone.url = milestoneData.second.get<std::string>("html_url");
    milestone.description = milestoneData.second.get<std::string>("description");
    milestone.openIssues = milestoneData.second.get<unsigned>("open_issues");
    milestone.closedIssues = milestoneData.second.get<unsigned>("closed_issues");
    milestone.isOpen = milestoneData.second.get<std::string>("state") == "open";
    milestone.createdAt = milestoneData.second.get<std::string>("created_at");
    milestone.updatedAt = milestoneData.second.get<std::string>("updated_at");
    milestone.dueOn = milestoneData.second.get<std::string>("due_on");
    milestone.closedAt = milestoneData.second.get<std::string>("closed_at");

    milestones.push_back(milestone);
  }
  return milestones;
}

std::vector<std::pair<std::string, unsigned>> GitHubDataConverter::ConvertContributors(pt::ptree& ptree)
{
  std::vector<std::pair<std::string, unsigned>>  contributors;
  for (pt::ptree::value_type &contributorData : ptree)
  {
    std::string username = contributorData.second.get<std::string>("login");
    unsigned contributions = contributorData.second.get<unsigned>("contributions");

    std::pair<std::string, unsigned> contributor{username, contributions};
    contributors.push_back(contributor);
  }
  return contributors;
}

model::Person GitHubDataConverter::ConvertUser(pt::ptree& ptree, unsigned contributions)
{
  model::Person user;

  user.username = ptree.get<std::string>("login");
  user.url = ptree.get<std::string>("html_url");
  user.name = ptree.get<std::string>("name");
  user.company = ptree.get<std::string>("company");
  user.contributions = contributions;
  user.type = ptree.get<std::string>("type");

  return user;
}

std::vector<model::Commit> GitHubDataConverter::ConvertCommits(pt::ptree& ptree)
{
  std::vector<model::Commit> commits;
  for (pt::ptree::value_type &commitData : ptree)
  {
    model::Commit commit;

    commit.sha = commitData.second.get<std::string>("sha");
    commit.message = commitData.second.get_child("commit").get<std::string>("message");
    commit.url = commitData.second.get<std::string>("html_url");
    commit.date = commitData.second.get_child("commit").get_child("author").get<std::string>("date");
    commit.comments = commitData.second.get_child("commit").get<unsigned>("comment_count");

    if (!commitData.second.get_child("author").empty())
    {
      util::OdbTransaction trans(_ctx.db);
      trans([&]
            {
              commit.user = (_ctx.db->query_one<model::Person>(
                odb::query<model::Person>::username == commitData.second.get_child("author").get<std::string>("login")));
            });
    }

    for (auto const &parent : commitData.second.get_child("parents"))
    {
      commit.parents.push_back(parent.second.get<std::string>("sha"));
    }
    commits.push_back(commit);
  }
  return commits;
}

std::vector<model::CommitFile> GitHubDataConverter::ConvertCommitFiles(pt::ptree& ptree, model::Commit& commit)
{
  LOG(debug) << "Processing files of commit(" << commit.sha << ").";
  commit.additions = ptree.get_child("stats").get<unsigned>("additions");
  commit.deletions = ptree.get_child("stats").get<unsigned>("deletions");
  commit.changes = ptree.get_child("stats").get<unsigned>("total");

  std::vector<model::CommitFile> commitFiles;
  for (pt::ptree::value_type &commitData : ptree.get_child("files"))
  {
    model::CommitFile commitFile;

    commitFile.sha = commitData.second.get<std::string>("sha");
    commitFile.path = commitData.second.get<std::string>("filename");
    commitFile.status = commitData.second.get<std::string>("status");
    commitFile.commitSha = commit.sha;
    commitFile.additions = commitData.second.get<unsigned>("additions");
    commitFile.deletions = commitData.second.get<unsigned>("deletions");
    commitFile.changes = commitData.second.get<unsigned>("changes");
    if (commitData.second.get_child_optional("patch").is_initialized())
    {
      commitFile.patch = commitData.second.get<std::string>("patch");
    }

    commitFiles.push_back(commitFile);
  }
  return commitFiles;
}

std::set<std::string> GitHubDataConverter::GetNewUsers(pt::ptree& ptree)
{
  std::set<std::string> newUsers;
  for (pt::ptree::value_type &data : ptree)
  {
    if (!data.second.get_child("user").empty())
    {
      util::OdbTransaction trans(_ctx.db);
      trans([&]
            {
              std::string username = data.second.get_child("user").get<std::string>("login");
              if (nullptr == (_ctx.db->query_one<model::Person>(
                odb::query<model::Person>::username == username)))
              {
                newUsers.insert(username);
              }
            });
    }
    if (data.second.get_child_optional("assignees").is_initialized() && !data.second.get_child("assignees").empty())
    {
      util::OdbTransaction trans(_ctx.db);
      for (pt::ptree::value_type &assigneeData: data.second.get_child("assignees"))
      {
        trans([&]
              {
                std::string username = assigneeData.second.get<std::string>("login");
                if (nullptr == (_ctx.db->query_one<model::Person>(
                  odb::query<model::Person>::username == username)))
                {
                  newUsers.insert(username);
                }
              });
      }
    }
  }

  return newUsers;
}

std::vector<model::Issue> GitHubDataConverter::ConvertIssues(pt::ptree& ptree)
{
  std::vector<model::Issue> issues;
  for (pt::ptree::value_type &issueData : ptree)
  {
    if (!issueData.second.get_child_optional("pull_request").is_initialized())
    {
      model::Issue issue;

      issue.number = issueData.second.get<std::uint64_t>("number");
      issue.title = issueData.second.get<std::string>("title");
      issue.body = issueData.second.get<std::string>("body");
      issue.url = issueData.second.get<std::string>("html_url");
      issue.isOpen = issueData.second.get<std::string>("state") == "open";
      issue.createdAt = issueData.second.get<std::string>("created_at");
      issue.updatedAt = issueData.second.get<std::string>("updated_at");
      issue.closedAt = issueData.second.get<std::string>("closed_at");

      if (!issueData.second.get_child("user").empty())
      {
        util::OdbTransaction trans(_ctx.db);
        trans([&]
              {
                issue.user = (_ctx.db->query_one<model::Person>(
                  odb::query<model::Person>::username == issueData.second.get_child("user").get<std::string>("login")));
              });
      }

      if (!issueData.second.get_child("milestone").empty())
      {
        util::OdbTransaction trans(_ctx.db);
        trans([&]
              {
                issue.milestone = (_ctx.db->query_one<model::Milestone>(
                  odb::query<model::Milestone>::number == issueData.second.get_child("milestone").get<unsigned>("number")));
              });
      }

      if (!issueData.second.get_child("labels").empty())
      {
        util::OdbTransaction trans(_ctx.db);
        for (pt::ptree::value_type &labelData: issueData.second.get_child("labels"))
        {
          trans([&]
                {
                  issue.labels.emplace_back(_ctx.db->query_one<model::Label>(
                    odb::query<model::Label>::id == labelData.second.get<std::uint64_t>("id")));
                });
        }
      }

      if (!issueData.second.get_child("assignees").empty())
      {
        util::OdbTransaction trans(_ctx.db);
        for (pt::ptree::value_type &assigneeData: issueData.second.get_child("assignees"))
        {
          trans([&]
                {
                  issue.assignees.emplace_back(_ctx.db->query_one<model::Person>(
                    odb::query<model::Person>::username == assigneeData.second.get<std::string>("login")));
                });
        }
      }

      issues.push_back(issue);
    }
  }
  return issues;
}

std::vector<model::Pull> GitHubDataConverter::ConvertPulls(pt::ptree& ptree)
{
  std::vector<model::Pull> pulls;
  for (pt::ptree::value_type &pullData : ptree)
  {
    model::Pull pull;

    pull.number = pullData.second.get<std::uint64_t>("number");
    pull.title = pullData.second.get<std::string>("title");
    pull.body = pullData.second.get<std::string>("body");
    pull.url = pullData.second.get<std::string>("html_url");
    pull.isOpen = pullData.second.get<std::string>("state") == "open";
    pull.createdAt = pullData.second.get<std::string>("created_at");
    pull.updatedAt = pullData.second.get<std::string>("updated_at");
    pull.closedAt = pullData.second.get<std::string>("closed_at");
    pull.mergedAt = pullData.second.get<std::string>("merged_at");

    if (!pullData.second.get_child("milestone").empty())
    {
      util::OdbTransaction trans(_ctx.db);
      trans([&]
            {
              pull.milestone = (_ctx.db->query_one<model::Milestone>(
                odb::query<model::Milestone>::number == pullData.second.get_child("milestone").get<unsigned>("number")));
            });
    }

    if (!pullData.second.get_child("labels").empty())
    {
      util::OdbTransaction trans(_ctx.db);
      for (pt::ptree::value_type &labelData: pullData.second.get_child("labels"))
      {
        trans([&]
              {
                pull.labels.emplace_back(_ctx.db->query_one<model::Label>(
                  odb::query<model::Label>::id == labelData.second.get<std::uint64_t>("id")));
              });
      }
    }
    if (!pullData.second.get_child("user").empty())
    {
      util::OdbTransaction trans(_ctx.db);
      trans([&]
            {
              pull.user = (_ctx.db->query_one<model::Person>(
                odb::query<model::Person>::username == pullData.second.get_child("user").get<std::string>("login")));
            });
    }

    pull.headLabel = pullData.second.get_child("head").get<std::string>("label");
    if (!pullData.second.get_child("head").get_child("repo").empty())
      pull.headRepoId = pullData.second.get_child("head").get_child("repo").get<std::uint64_t>("id");
    pull.baseLabel = pullData.second.get_child("base").get<std::string>("label");
    if (!pullData.second.get_child("base").get_child("repo").empty())
      pull.baseRepoId = pullData.second.get_child("base").get_child("repo").get<std::uint64_t>("id");

    pulls.push_back(pull);
  }
  return pulls;
}

std::vector<model::PullFile> GitHubDataConverter::ConvertPullFiles(pt::ptree& ptree, model::Pull& pull)
{
    LOG(debug) << "Processing files for pull no." << pull.number << ".";

    std::vector<model::PullFile> pullFiles;
    for (pt::ptree::value_type &pullFileData : ptree)
    {
      model::PullFile pullFile;

      pullFile.sha = pullFileData.second.get<std::string>("sha");
      pullFile.path = pullFileData.second.get<std::string>("filename");
      pullFile.status = pullFileData.second.get<std::string>("status");
      pullFile.prNumber = pull.number;
      pullFile.additions = pullFileData.second.get<unsigned>("additions");
      pullFile.deletions = pullFileData.second.get<unsigned>("deletions");
      pullFile.changes = pullFileData.second.get<unsigned>("changes");
      if (pullFileData.second.get_child_optional("patch").is_initialized())
      {
        pullFile.patch = pullFileData.second.get<std::string>("patch");
      }

      pullFiles.push_back(pullFile);
    }
    return pullFiles;
}

model::Pull GitHubDataConverter::ConvertPull(pt::ptree& ptree, model::Pull& pull)
{
  pull.isMerged = ptree.get<std::string>("merged") == "true";
  pull.mergeable = ptree.get<std::string>("mergeable");
  pull.mergeableState = ptree.get<std::string>("mergeable_state");
  pull.commentCount = ptree.get<unsigned>("comments");
  pull.reviewCommentCount = ptree.get<unsigned>("review_comments");
  pull.commitCount = ptree.get<unsigned>("commits");
  pull.additions = ptree.get<unsigned>("additions");
  pull.deletions = ptree.get<unsigned>("deletions");
  pull.changedFiles = ptree.get<unsigned>("changed_files");

  if (!ptree.get_child("merged_by").empty())
  {
    util::OdbTransaction trans(_ctx.db);
    trans([&]
          {
            pull.mergedBy = (_ctx.db->query_one<model::Person>(
              odb::query<model::Person>::username == ptree.get_child("merged_by").get<std::string>("login")));
          });
  }

  if (!ptree.get_child("assignees").empty())
  {
    util::OdbTransaction trans(_ctx.db);
    for (pt::ptree::value_type &assigneeData: ptree.get_child("assignees"))
    {
      trans([&]
            {
              pull.assignees.emplace_back(_ctx.db->query_one<model::Person>(
                odb::query<model::Person>::username == assigneeData.second.get<std::string>("login")));
            });
    }
  }

  return pull;
}

std::vector<model::Review> GitHubDataConverter::ConvertPullReviews(pt::ptree& ptree, model::Pull& pull)
{
  LOG(debug) << "Processing reviews for pull no." << pull.number << ".";

  std::vector<model::Review> reviews;
  for (pt::ptree::value_type &reviewData : ptree)
  {
    model::Review review;
    review.id = reviewData.second.get<std::uint64_t>("id");
    review.state = reviewData.second.get<std::string>("state");
    review.body = reviewData.second.get<std::string>("body");
    review.url = reviewData.second.get<std::string>("html_url");
    review.submittedAt = reviewData.second.get<std::string>("submitted_at");

    if (!reviewData.second.get_child("user").empty())
    {
      util::OdbTransaction trans(_ctx.db);
      trans([&]
            {
              review.user = (_ctx.db->query_one<model::Person>(
                odb::query<model::Person>::username == reviewData.second.get_child("user").get<std::string>("login")));
              pull.reviewers.emplace_back((_ctx.db->query_one<model::Person>(
                odb::query<model::Person>::username == reviewData.second.get_child("user").get<std::string>("login"))));
            });
    }

    pull.reviews.emplace_back(std::make_shared<model::Review>(review));
    reviews.push_back(review);
  }
  return reviews;
}

std::vector<model::Comment> GitHubDataConverter::ConvertPullComments(pt::ptree& ptree, model::Pull& pull)
{
  LOG(debug) << "Processing comments for pull no." << pull.number << ".";

  std::vector<model::Comment> comments;
  for (pt::ptree::value_type &commentData : ptree)
  {
    model::Comment comment;

    comment.id = commentData.second.get<std::uint64_t>("id");
    comment.pullReviewId = commentData.second.get<std::uint64_t>("pull_request_review_id");
    comment.commitId = commentData.second.get<std::string>("commit_id");
    comment.originalCommitId = commentData.second.get<std::string>("original_commit_id");
    comment.diffHunk = commentData.second.get<std::string>("diff_hunk");
    comment.path = commentData.second.get<std::string>("path");
    comment.body = commentData.second.get<std::string>("body");
    comment.url = commentData.second.get<std::string>("html_url");
    comment.createdAt = commentData.second.get<std::string>("created_at");
    comment.updatedAt = commentData.second.get<std::string>("updated_at");

    if (!commentData.second.get_child("user").empty())
    {
      util::OdbTransaction trans(_ctx.db);
      trans([&]
            {
              comment.user = (_ctx.db->query_one<model::Person>(
                odb::query<model::Person>::username == commentData.second.get_child("user").get<std::string>("login")));
            });
    }
    pull.reviewComments.emplace_back(std::make_shared<model::Comment>(comment));
    comments.push_back(comment);
  }
  return comments;
}

}
}
