#ifndef CC_MODEL_YAMLEDGE_H
#define CC_MODEL_YAMLEDGE_H

#include <string>

#include <model/microservice.h>
#include "model/helmtemplate.h"

#include <util/hash.h>

namespace cc
{
namespace model
{

struct DependencyEdge;
typedef std::shared_ptr<DependencyEdge> DependencyEdgePtr;

typedef std::uint64_t DependencyEdgeId;

#pragma db object
struct DependencyEdge
{
  #pragma db id
  DependencyEdgeId id;

  #pragma db not_null
  uint64_t helperId;

  #pragma db not_null
  #pragma db on_delete(cascade)
  std::shared_ptr<Microservice> from;

  #pragma db not_null
  #pragma db on_delete(cascade)
  std::shared_ptr<Microservice> to;

  #pragma db not_null
  #pragma db on_delete(cascade)
  std::shared_ptr<HelmTemplate> connection;

  #pragma db not_null
  std::string type;

  std::string toString() const;
};

inline std::string DependencyEdge::toString() const
{
  return std::string("DependencyEdge")
    .append("\nfrom = ").append(std::to_string(from->serviceId))
    .append("\nto = ").append(std::to_string(to->serviceId))
    .append("\ntype = ");
}

inline std::uint64_t createIdentifier(const DependencyEdge& edge_)
{
  return util::fnvHash(
    std::to_string(edge_.from->serviceId) +
    std::to_string(edge_.to->serviceId) +
    std::to_string(edge_.helperId) +
    edge_.type);
}

}
}

#endif // CC_MODEL_YAMLEDGE_H
