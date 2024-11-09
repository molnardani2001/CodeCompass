#ifndef CODECOMPASS_KAFKATOPIC_H
#define CODECOMPASS_KAFKATOPIC_H

#include <odb/core.hxx>
#include <odb/lazy-ptr.hxx>
#include <odb/nullable.hxx>

#include "model/file.h"

#include "util/hash.h"

namespace cc
{
namespace model
{

typedef std::uint64_t KafkaTopicId;

#pragma db object
struct Kafkatopic
{
  #pragma db id
  KafkaTopicId topicId;

  #pragma db not_null
  std::string name;

  std::string topicName;

  std::uint64_t replicaCount;

  std::uint64_t partitionCount;
};
}
}
#endif //CODECOMPASS_KAFKATOPIC_H
