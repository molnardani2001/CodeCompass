#ifndef CC_MODEL_MICROSERVICE_H
#define CC_MODEL_MICROSERVICE_H

#include <odb/core.hxx>
#include <odb/lazy-ptr.hxx>
#include <odb/nullable.hxx>

#include "model/file.h"

#include "util/hash.h"

namespace cc
{
namespace model
{

typedef std::uint64_t MicroserviceId;

#pragma db object
struct Microservice
{
  enum class ServiceType
  {
    PRODUCT,
    INTEGRATION,
    CENTRAL
  };

  #pragma db id
  MicroserviceId serviceId;

  #pragma db not_null
  std::string name;

  std::string version;

  FileId file;

  ServiceType type;

  std::uint8_t podCount;
};

inline std::uint64_t createIdentifier(const Microservice& service_)
{
  return util::fnvHash(
    service_.name +
    service_.version);
}
}
}

#endif // CC_MODEL_MICROSERVICE_H
