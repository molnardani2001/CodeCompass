namespace cpp cc.service.github
namespace java cc.service.github

struct Person
{
  1:string username
  2:string name
  3:string company
  4:string url
}

struct Pull
{
  1:i32 number
  2:string title
  3:string body
  4:string user
  5:string createdAt
  6:string headLabel
  7:string baseLabel
  8:bool isOpen
  9:bool isMerged
  10:string mergedBy
}

struct Label
{
  1:i64 id
  2:string name
}

struct PullFile
{
  1:string sha
  2:string path
}

service GitHubService
{
  string getGitHubString()
  list<Pull> getPullList()
  list<PullFile> getFileListForPull(1:i64 pullNum_)
  list<Person>getReviewerListForPull(1:i64 pullNum_)
  list<Label>getLabelListForPull(1:i64 pullNum_)
  list<Person> getContributorList()
  list<Pull> getPullListForAuthor(1:string user)
  list<Pull> getPullListForReviewer(1:string user)
}