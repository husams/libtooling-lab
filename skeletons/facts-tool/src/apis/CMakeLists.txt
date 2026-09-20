include(${PROJECT_SOURCE_DIR}/cmake/ApiDependencies.cmake)
set(FACTS_BOOST_INCLUDE_DIR "${FACTS_BOOST_INCLUDE_DIR}" PARENT_SCOPE)
set(FACTS_JSON_INCLUDE_DIR "${FACTS_JSON_INCLUDE_DIR}" PARENT_SCOPE)
find_package(Threads REQUIRED)

add_library(facts-apis STATIC
  Server.cpp
  config/Defaults.cpp
  config/Encode.cpp
  config/Merge.cpp
  config/Options.cpp
  config/Parse.cpp
  config/Paths.cpp
  config/Read.cpp
  config/Write.cpp
  config/Watch.cpp
  daemon/Detach.cpp
  daemon/Lifecycle.cpp
  daemon/Lock.cpp
  daemon/Readiness.cpp
  daemon/Redirect.cpp
  generated/Spec.cpp
  http/Access.cpp
  http/Arguments.cpp
  http/Listener.cpp
  http/OpenApi.cpp
  http/OpenApiYaml.cpp
  http/Response.cpp
  http/Router.cpp
  http/Routing.cpp
  http/Session.cpp
  http/Submission.cpp
  jobs/Process.cpp
  jobs/ProcessOutput.cpp
  jobs/ProcessWait.cpp
  jobs/Queue.cpp
  jobs/QueueCancel.cpp
  jobs/QueueRecords.cpp
  jobs/QueueRun.cpp
  jobs/Spawn.cpp
  watch/Apply.cpp
  watch/Events.cpp
  watch/Filter.cpp
  watch/Paths.cpp
  watch/Refresh.cpp
  watch/Rescan.cpp
  watch/Scan.cpp
  watch/Status.cpp
  watch/Update.cpp
  watch/Watcher.cpp
  watch/catalog/Read.cpp
  watch/catalog/Exclusions.cpp
  watch/ignore/Ignore.cpp
  watch/ignore/Repository.cpp
  watch/ignore/Controls.cpp
  watch/ignore/Rules.cpp
  watch/ignore/Layered.cpp
  watch/plan/Options.cpp
  watch/plan/Sources.cpp
  watch/plan/Selection.cpp
  watch/plan/Batches.cpp
  watch/plan/Plan.cpp
)
target_include_directories(facts-apis PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(facts-apis
  PUBLIC Boost::headers nlohmann_json::nlohmann_json Threads::Threads
  PRIVATE CLI11::CLI11 yaml-cpp::yaml-cpp facts-config facts-storage
          facts-tooling facts-clang facts-git)
target_compile_definitions(facts-apis PRIVATE BOOST_ERROR_CODE_HEADER_ONLY)
