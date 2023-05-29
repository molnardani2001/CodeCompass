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
  1:i64 number
  2:string title
  3:string body
  4:string user
  5:string createdAt
  6:string updatedAt
  7:string headLabel
  8:string baseLabel
  9:bool isOpen
  10:bool isMerged
  11:string mergedBy
  12:string closedAt
  13:string mergedAt
  14:i32 additions
  15:i32 deletions
  16:i32 changedFiles
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
  3:string status
  4:i64 prNumber
  5:i32 additions
  6:i32 deletions
  7:i32 changes
  8:string patch
}

struct Review
{
  1:i64 id
  2:string user
  3:string state
  4:string body
  5:string url
  6:string submittedAt
}

struct Comment
{
  1:i64 id
  2:i64 pullReviewId
  3:string commitId
  4:string originalCommitId
  5:string user
  6:string diffHunk
  7:string path
  8:string body
  9:string url
  10:string createdAt
  11:string updatedAt
}

struct CommentsByDiffHunk
{
  1:string key
  2:list<Comment> value
}

struct CommentCountPerAuthor
{
  1:string key
  2:i64 value
}

struct Contribution
{
  1:string user
  2:i32 comments
  3:i32 changeRequests
  4:i32 approves
}

service GitHubService
{
  string getGitHubString()

  Pull getPull(1:i64 pullNum);
  list<Pull> getPullList()
  list<Person> getContributorList()

  list<PullFile> getFileListForPull(1:i64 pullNum)
  list<Person> getReviewerListForPull(1:i64 pullNum)
  list<Label> getLabelListForPull(1:i64 pullNum)

  list<Contribution> getContributionsForPull(1:i64 pullNum)
  Contribution getContributionsForPullByAuthor(1:i64 pullNum, 2:string username)

  i64 getCommentCountForPullFile(1:i64 pullNum, 2:string path)
  list<Comment> getCommentsForPullFile(1:i64 pullNum, 2:string path)
  list<CommentsByDiffHunk> getCommentsForPullFilePerDiffHunk(1:i64 pullNum, 2:string path)
  list<CommentCountPerAuthor> getCommentCountForPullFilePerAuthor (1:i64 pullNum, 2:string path)

  list<Pull> getPullListForAuthor(1:string user)
  list<Pull> getPullListForReviewer(1:string user)
}