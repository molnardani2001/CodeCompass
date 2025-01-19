#ifndef CODECOMPASS_CHARTDEPENDENCYEDGE_H
#define CODECOMPASS_CHARTDEPENDENCYEDGE_H

#include <odb/core.hxx>
#include <model/chart.h>
#include <util/hash.h>

namespace cc
{
namespace model
{
struct ChartDependencyEdge;
typedef std::shared_ptr<ChartDependencyEdge> ChartDependencyEdgePtr;

typedef std::uint64_t ChartDependencyEdgeId;

// Class to model dependencies between charts
#pragma db object
struct ChartDependencyEdge
{
  #pragma db id
  ChartDependencyEdgeId id;

  #pragma db not_null
  #pragma db on_delete(cascade)
  std::shared_ptr<Chart> from;

  #pragma db not_null
  #pragma db on_delete(cascade)
  std::shared_ptr<Chart> to;

  std::string toString() const;
};

inline std::string ChartDependencyEdge::toString() const
{
  return std::string("ChartDependencyEdge")
    .append("\nfrom = ").append(std::to_string(from->chartId))
    .append("\nto = ").append(std::to_string(to->chartId));
}

inline std::uint64_t createIdentifier(const ChartDependencyEdge& chartDependencyEdge_)
{
 return util::fnvHash(
   std::to_string(chartDependencyEdge_.from->chartId) +
   std::to_string(chartDependencyEdge_.to->chartId)
   );
}
}
}

#endif //CODECOMPASS_CHARTDEPENDENCYEDGE_H
