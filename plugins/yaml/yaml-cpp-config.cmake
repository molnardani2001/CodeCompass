# - Config file for the yaml-cpp package
# It defines the following variables
#  YAML_CPP_INCLUDE_DIR - include directory
#  YAML_CPP_LIBRARIES    - libraries to link against

@PACKAGE_INIT@

set_and_check(YAML_CPP_INCLUDE_DIR "@PACKAGE_CMAKE_INSTALL_INCLUDEDIR@")

# Our library dependencies (contains definitions for IMPORTED targets)
include(@PACKAGE_CONFIG_EXPORT_DIR@/yaml-cpp-targets.cmake)

# These are IMPORTED targets created by yaml-cpp-targets.cmake
set(YAML_CPP_LIBRARIES "@EXPORT_TARGETS@")

check_required_components(@EXPORT_TARGETS@)