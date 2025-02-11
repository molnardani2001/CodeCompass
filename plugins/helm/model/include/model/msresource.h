#ifndef CC_MODEL_MSRESOURCE_H
#define CC_MODEL_MSRESOURCE_H

#include <odb/core.hxx>
#include <odb/lazy-ptr.hxx>
#include <odb/nullable.hxx>
#include <boost/lexical_cast.hpp>
#include "util/hash.h"
#include <model/helmtemplate.h>

namespace cc
{
namespace model
{

typedef uint64_t MSResouceId;

#pragma db object
struct MSResource
{
  enum class ResourceType
  {
    CPU,
    MEMORY,
    STORAGE
  };

  #pragma db id auto
  MSResouceId id;

  #pragma db not_null
  ResourceType type;

  #pragma db not_null
  HelmTemplateId helmTemplateId;

  #pragma db not_null
  float requestedAmount;

  #pragma db not_null
  float limitAmount;

  #pragma db not_null
  std::string unit;
};

inline std::string resourceTypeToString(MSResource::ResourceType type_)
{
  switch (type_)
  {
    case MSResource::ResourceType::CPU: return "CPU";
    case MSResource::ResourceType::MEMORY: return "Memory";
    case MSResource::ResourceType::STORAGE: return "Storage";
  }

  return std::string();
}
}
}

#endif // CC_MODEL_MSRESOURCE_H
