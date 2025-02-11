#ifndef CC_MODEL_HELM_H
#define CC_MODEL_HELM_H

#include <string>

#include "yaml-cpp/yaml.h"

#include <odb/core.hxx>
#include <odb/lazy-ptr.hxx>
#include <odb/nullable.hxx>

#include <model/file.h>
#include <model/microservice.h>
#include <model/chart.h>

namespace cc
{
namespace model
{

typedef uint64_t HelmTemplateId;

#pragma db object polymorphic
struct HelmTemplate
{
  enum class TemplateType
  {
    DEPLOYMENT,
    SERVICE,
    CONFIGMAP,
    SECRET,
    KAFKATOPIC,
    STATEFULSET,
    DAEMONSET,
    POD,
    OTHER
  };

  #pragma db id
  HelmTemplateId id;

  #pragma db not_null
  FileId file;

  std::string name;

  #pragma db not_null
  TemplateType templateType;

  #pragma db not_null
  std::string kind;

  ChartId depends;
  bool operator==(HelmTemplate& rhs);

  virtual ~HelmTemplate() = default;
};

inline std::uint64_t createIdentifier(const HelmTemplate& helm_)
{
  return util::fnvHash(
    helm_.name +
    helm_.kind +
    std::to_string(helm_.file));
}

// Deployments, DaemonSets and StatefulSets are considered as workloads
// the pods managed by their manifest file are going to be actual microservices
#pragma db object polymorphic
struct Workload : public HelmTemplate
{
  std::string labels;

  std::string images;

  std::string ports;

  // possibly std::shared_ptr<model::Microservice>
  MicroserviceId microservice;

  virtual ~Workload() override = default;
};

#pragma db object
struct Deployment : public Workload
{
  int replicas;
};

#pragma db object
struct DaemonSet : public Workload
{
  std::string tolerations;

  std::string nodeSelector;
};

#pragma db object
struct StatefulSet : public Workload
{
  int replicas;
};

#pragma db object
struct Service : public HelmTemplate
{
  std::string ipFamilyPolicy;

  std::string type;

  std::string ports;

  std::string selector;
};

#pragma db object
struct ConfigMap : public HelmTemplate {};

#pragma db object
struct Secret : public HelmTemplate {};

typedef std::uint64_t KafkaTopicId;

#pragma db object
struct KafkaTopic : public HelmTemplate
{
  std::string topicName;

  std::uint64_t replicaCount;

  std::uint64_t partitionCount;
};

#pragma db object
struct NetworkPolicy : public HelmTemplate
{

};
}
}

#endif // CC_MODEL_HELM_H
