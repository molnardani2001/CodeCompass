#ifndef CODECOMPASS_SERVICE_H
#define CODECOMPASS_SERVICE_H

#include <odb/core.hxx>
#include <odb/nullable.hxx>

#include "model/file.h"
#include "model/microservice.h"
#include "model/helmtemplate.h"

#include "util/hash.h"

namespace cc
{
namespace model
{

typedef std::uint64_t ServiceId;

#pragma db object
struct Service
{
  #pragma db id
  ServiceId serviceId;

  #pragma db not_null
  std::string name;

  std::string ipFamilyPolicy;

  std::string type;

  #pragma db not_null
  MicroserviceId depends;

  #pragma db not_null
  HelmTemplateId helmTemplateId;
  //TODO: consider ports here as well
};

inline std::uint64_t createIdentifier(const Service& service_)
{
  return util::fnvHash(
    service_.name + service_.type
    );
}
}
}

#endif //CODECOMPASS_SERVICE_H
