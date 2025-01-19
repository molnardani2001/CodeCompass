set(ODB_SOURCES
  include/model/helmtemplate.h
  include/model/microservice.h
  include/model/dependencyedge.h
  include/model/msresource.h
  include/model/yamlastnode.h
  include/model/yamlfile.h
  include/model/yamlcontent.h
  include/model/kafkatopic.h
  include/model/service.h
  include/model/chart.h
  include/model/chartdependencyedge.h)

generate_odb_files("${ODB_SOURCES}")

add_odb_library(yamlmodel ${ODB_CXX_SOURCES})
add_dependencies(yamlmodel model)

install_sql()
