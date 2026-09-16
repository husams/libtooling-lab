add_library(facts-ast-cache STATIC
  Cache.cpp
  ExternalInputs.cpp
  FileIdentity.cpp
  Fingerprint.cpp
  Includes.cpp
  LookupPaths.cpp
  Metadata.cpp
  Parse.cpp
  Storage.cpp
)
target_include_directories(facts-ast-cache PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(facts-ast-cache PUBLIC facts-clang)
