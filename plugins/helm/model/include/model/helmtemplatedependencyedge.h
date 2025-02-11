#ifndef CODECOMPASS_HELMTEPLATEDEPENDENCYEDGE_H
#define CODECOMPASS_HELMTEPLATEDEPENDENCYEDGE_H
#include <odb/core.hxx>
#include <model/helmtemplate.h>
#include <util/hash.h>

namespace cc
{
namespace model
{
struct HelmTemplateDependencyEdge;
typedef std::shared_ptr<HelmTemplateDependencyEdge> HelmTemplateDependencyEdgePtr;

typedef std::uint64_t HelmTemplateDependencyEdgeId;

// Class to model dependencies between template files
#pragma db object
struct HelmTemplateDependencyEdge
{
  #pragma db id
  HelmTemplateDependencyEdgeId id;

  #pragma db not_null
  #pragma db on_delete(cascade)
  std::shared_ptr<model::HelmTemplate> from;

  #pragma db not_null
  #pragma db on_delete(cascade)
  std::shared_ptr<model::HelmTemplate> to;

  std::string toString() const;
};

inline std::string HelmTemplateDependencyEdge::toString() const
{
  return std::string("HelmTemplateDependencyEdge")
    .append("\nfrom = ").append(std::to_string(from->id))
    .append("\nto = ").append(std::to_string(to->id));
}

inline std::uint64_t createIdentifier(const HelmTemplateDependencyEdgePtr& helmTemplateDependencyEdge_)
{
  return util::fnvHash(
    std::to_string(helmTemplateDependencyEdge_->from->id) +
      std::to_string(helmTemplateDependencyEdge_->to->id)
  );
}
}
}
#endif //CODECOMPASS_HELMTEPLATEDEPENDENCYEDGE_H
