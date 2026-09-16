add_library(facts-ast-cache STATIC
  Cache.cpp
  FileIdentity.cpp
  Fingerprint.cpp
  FingerprintArguments.cpp
  Includes.cpp
  Metadata.cpp
  Parse.cpp
  Preprocess.cpp
  PreprocessAction.cpp
  RevisionObserver.cpp
  Revisions.cpp
  SearchDirectories.cpp
  Serialization.cpp
  Snapshot.cpp
  SnapshotGeneration.cpp
  Storage.cpp
)
target_include_directories(facts-ast-cache PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(facts-ast-cache PUBLIC facts-clang
  PRIVATE facts-storage facts-config facts-git facts-ast)
