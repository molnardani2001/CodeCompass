include_directories(
  include
  ${PROJECT_SOURCE_DIR}/model/include
  ${PROJECT_SOURCE_DIR}/util/include
  ${PROJECT_SOURCE_DIR}/parser/include
  ${PLUGIN_DIR}/model/include
  ${PLUGIN_DIR})

add_library(githubparser SHARED
  src/githubparser.cpp
        src/githubdataconverter.cpp
        include/githubparser/githubdataconverter.h)

target_link_libraries(githubparser
  githubmodel
  util
  ${Boost_LIBRARIES})

target_compile_options(githubparser PUBLIC -Wno-unknown-pragmas)

install(TARGETS githubparser DESTINATION ${INSTALL_PARSER_DIR})
