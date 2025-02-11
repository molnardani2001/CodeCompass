#ifndef CODECOMPASS_CHART_H
#define CODECOMPASS_CHART_H

#include <odb/core.hxx>
#include <odb/lazy-ptr.hxx>
#include <odb/nullable.hxx>
#include <string>

#include "model/file.h"
#include <model/microservice.h>

#include "util/hash.h"

namespace cc
{
namespace model
{

typedef std::uint64_t ChartId;

// Class to model actual charts
#pragma db object
struct Chart
{
  enum class ChartType
  {
    INTEGRATION_CHART,
    STANDALONE_CHART,
    SUB_CHART,
    SUB_INTEGRATION_CHART
  };

  #pragma db id
  ChartId chartId;

  FileId file;

  #pragma db not_null
  std::string name;

  #pragma db null
  std::string alias;

  std::string version;

  #pragma db not_null
  ChartType type;

  #pragma db null
  MicroserviceId microservice;
};

inline std::uint64_t createIdentifier(const Chart& chart_)
{
  return util::fnvHash(
    chart_.name + chart_.alias + chart_.version);
}

}
}

#endif //CODECOMPASS_CHART_H
