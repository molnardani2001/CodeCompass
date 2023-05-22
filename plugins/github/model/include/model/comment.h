#ifndef CC_MODEL_COMMENT_H
#define CC_MODEL_COMMENT_H

#include <cstdint>
#include <string>

#include <odb/core.hxx>
#include <odb/lazy-ptr.hxx>

#include <model/person.h>

namespace cc
{
namespace model
{

#pragma db object
struct Comment
{
  #pragma db id
  std::uint64_t id;

  #pragma db not_null
  std::uint64_t pullReviewId;

  #pragma db not_null
  std::string commitId;

  #pragma db not_null
  std::string originalCommitId;

  #pragma db null
  odb::lazy_shared_ptr<Person> user;

  #pragma db not_null
  std::string diffHunk;

  #pragma db not_null
  std::string path;

  #pragma db not_null
  std::string body;

  #pragma db not_null
  std::string url;

  #pragma db not_null
  std::string createdAt;

  #pragma db not_null
  std::string updatedAt;
};
} // model
} // cc

#endif // CC_MODEL_COMMENT_H
