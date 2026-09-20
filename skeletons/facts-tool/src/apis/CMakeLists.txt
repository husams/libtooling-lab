find_package(Boost 1.74 REQUIRED)
find_package(nlohmann_json 3.9 REQUIRED)
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
  daemon/Detach.cpp
  daemon/Lifecycle.cpp
  daemon/Lock.cpp
  daemon/Readiness.cpp
  daemon/Redirect.cpp
  http/Arguments.cpp
  http/Listener.cpp
  http/OpenApi.cpp
  http/Response.cpp
  http/Router.cpp
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
  watch/Commands.cpp
  watch/Events.cpp
  watch/Paths.cpp
  watch/Refresh.cpp
  watch/Rescan.cpp
  watch/Scan.cpp
  watch/Status.cpp
  watch/Watcher.cpp
)
target_include_directories(facts-apis PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(facts-apis
  PUBLIC Boost::headers nlohmann_json::nlohmann_json Threads::Threads
  PRIVATE CLI11::CLI11 yaml-cpp::yaml-cpp)
target_compile_definitions(facts-apis PRIVATE BOOST_ERROR_CODE_HEADER_ONLY)
