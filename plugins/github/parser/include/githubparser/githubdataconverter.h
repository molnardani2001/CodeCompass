#ifndef CC_PARSER_GITHUBDATACONVERTER_H
#define CC_PARSER_GITHUBDATACONVERTER_H

#include <vector>
#include <string>

#include <boost/property_tree/ptree.hpp>
#include <odb/database.hxx>

#include "parser/parsercontext.h"

#include "model/label.h"
#include "model/milestone.h"
#include "model/person.h"
#include "model/commit.h"
#include "model/commitfile.h"
#include "model/issue.h"
#include "model/pull.h"
#include "model/review.h"
#include "model/comment.h"
#include "model/pullfile.h"

namespace cc
{
namespace parser
{
namespace pt = boost::property_tree;

class GitHubDataConverter
{
public:
  GitHubDataConverter(ParserContext& ctx);

  std::vector<model::Label> ConvertLabels(pt::ptree& ptree);
  std::vector<model::Milestone> ConvertMilestones(pt::ptree& ptree);
  std::vector<std::pair<std::string, unsigned>> ConvertContributors(pt::ptree& ptree);
  model::Person ConvertUser(pt::ptree& ptree, unsigned contributions);
  std::vector<model::Commit> ConvertCommits(pt::ptree& ptree);
  std::vector<model::CommitFile> ConvertCommitFiles(pt::ptree& ptree, model::Commit& commit);
  std::set<std::string> GetNewUsers(pt::ptree& ptree);
  std::vector<model::Issue> ConvertIssues(pt::ptree& ptree);
  std::vector<model::Pull> ConvertPulls(pt::ptree& ptree);
  std::vector<model::PullFile> ConvertPullFiles(pt::ptree& ptree, model::Pull& pull);
  std::vector<model::Review> ConvertPullReviews(pt::ptree& ptree, model::Pull& pull);
  std::vector<model::Comment> ConvertPullComments(pt::ptree& ptree, model::Pull& pull);
  model::Pull ConvertPull(pt::ptree& ptree, model::Pull& pull);
private:
  ParserContext& _ctx;
};

}
}

#endif
