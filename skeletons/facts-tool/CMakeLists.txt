# facts-tool — skeleton for a fact-extraction tool over the Clang AST.
#
# Configure & build (macOS, Homebrew LLVM):
#   cmake -G Ninja -B build \
#     -DCMAKE_PREFIX_PATH=$(brew --prefix llvm) \
#     -DCMAKE_CXX_COMPILER=$(brew --prefix llvm)/bin/clang++ .
#   cmake --build build
cmake_minimum_required(VERSION 3.20)
# C is needed too: LLVM's own cmake modules (FindLibEdit) run C header checks.
project(facts_tool C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)
find_package(SQLite3 REQUIRED)
message(STATUS "Building against LLVM ${LLVM_PACKAGE_VERSION} at ${LLVM_DIR}")

# LLVM and Clang are implementation dependencies shared by the internal modules.
add_library(facts-clang INTERFACE)
target_include_directories(facts-clang SYSTEM INTERFACE
  ${LLVM_INCLUDE_DIRS}
  ${CLANG_INCLUDE_DIRS}
)
target_compile_options(facts-clang INTERFACE ${LLVM_DEFINITIONS})
if(NOT LLVM_ENABLE_RTTI)
  target_compile_options(facts-clang INTERFACE -fno-rtti)
endif()
target_link_libraries(facts-clang INTERFACE clang-cpp LLVM)

# Domain facts are header-only value types shared by extraction and storage.
add_library(facts-model INTERFACE)
target_include_directories(facts-model INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(facts-model INTERFACE facts-clang)

add_library(facts-storage STATIC
  src/storage/FileDatabase.cpp
  src/storage/FileManager.cpp
  src/storage/Definition.cpp
  src/storage/Enumeration.cpp
  src/storage/FactStore.cpp
  src/storage/Function.cpp
  src/storage/FunctionInstance.cpp
  src/storage/FunctionTemplate.cpp
  src/storage/Parameter.cpp
  src/storage/Record.cpp
  src/storage/RecordInstance.cpp
  src/storage/RecordTemplate.cpp
  src/storage/Relation.cpp
  src/storage/SchemaMigration.cpp
  src/storage/Sqlite.cpp
  src/storage/Storage.cpp
  src/storage/Symbol.cpp
  src/storage/SymbolIdentity.cpp
  src/storage/Variable.cpp
)
target_include_directories(facts-storage PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(facts-storage
  PUBLIC facts-model
  PRIVATE facts-clang SQLite3::SQLite3
)

add_library(facts-tooling STATIC
  src/tooling/CompilationFiles.cpp
)
target_include_directories(facts-tooling PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(facts-tooling PRIVATE facts-clang)

add_library(facts-ast STATIC
  src/ast/FactExtractor.cpp
  src/ast/extractors/Definition.cpp
  src/ast/extractors/FieldDecl.cpp
  src/ast/extractors/File.cpp
  src/ast/extractors/FunctionDecl.cpp
  src/ast/extractors/FunctionTemplate.cpp
  src/ast/extractors/Initializer.cpp
  src/ast/extractors/Location.cpp
  src/ast/extractors/MethodDecl.cpp
  src/ast/extractors/NamedDecl.cpp
  src/ast/extractors/ParmVarDecl.cpp
  src/ast/extractors/Parameters.cpp
  src/ast/extractors/RecordDecl.cpp
  src/ast/extractors/RecordInstance.cpp
  src/ast/extractors/TemplatePattern.cpp
  src/ast/extractors/TemplateSpecialization.cpp
  src/ast/extractors/Type.cpp
  src/ast/extractors/ValueDecl.cpp
  src/ast/extractors/VarDecl.cpp
  src/ast/visitors/SymbolCollector.cpp
  src/ast/visitors/SymbolVisitor.cpp
  src/ast/visitors/Traversal.cpp
)
target_include_directories(facts-ast PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(facts-ast
  PUBLIC facts-model
  PRIVATE facts-storage facts-clang
)

add_library(facts-platform STATIC
  src/platform/PlatformFlags.cpp
)
target_include_directories(facts-platform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(facts-platform PRIVATE facts-clang)

# Keep the executable as the composition root; application logic lives in modules.
add_executable(facts-tool src/main.cpp)
target_link_libraries(facts-tool PRIVATE
  facts-ast
  facts-storage
  facts-platform
  facts-tooling
  facts-clang
)

include(CTest)
if(BUILD_TESTING)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  execute_process(
    COMMAND ${Python3_EXECUTABLE} -c "import pytest, pytest_bdd"
    RESULT_VARIABLE FACTS_TOOL_PYTEST_IMPORT_RESULT
    OUTPUT_QUIET
    ERROR_QUIET
  )
  if(NOT FACTS_TOOL_PYTEST_IMPORT_RESULT EQUAL 0)
    message(FATAL_ERROR
      "pytest and pytest-bdd are required when BUILD_TESTING is enabled; "
      "install tests/e2e/requirements.txt"
    )
  endif()

  add_executable(fact-store-test tests/FactStoreTest.cpp)
  target_link_libraries(fact-store-test PRIVATE facts-storage facts-model
                                              SQLite3::SQLite3)
  add_test(
    NAME fact-store
    COMMAND fact-store-test ${CMAKE_CURRENT_BINARY_DIR}/fact-store-test.sqlite
  )
  add_executable(file-manager-test tests/FileManagerTest.cpp)
  target_link_libraries(file-manager-test PRIVATE facts-storage facts-model
                                                facts-clang SQLite3::SQLite3)
  add_test(
    NAME file-manager
    COMMAND file-manager-test
            ${CMAKE_CURRENT_BINARY_DIR}/file-manager-test.sqlite
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/tu_one.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/shared.h
  )
  add_executable(compilation-files-test tests/CompilationFilesTest.cpp)
  target_link_libraries(compilation-files-test PRIVATE facts-tooling
                                                       facts-clang)
  add_test(
    NAME compilation-files
    COMMAND compilation-files-test
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/tu_one.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures
  )
  add_executable(storage-schema-test tests/StorageSchemaTest.cpp)
  target_link_libraries(storage-schema-test PRIVATE facts-storage facts-model
                                                    SQLite3::SQLite3)
  add_test(
    NAME storage-schema
    COMMAND storage-schema-test
            ${CMAKE_CURRENT_BINARY_DIR}/storage-schema-fresh.sqlite
            ${CMAKE_CURRENT_BINARY_DIR}/storage-schema-legacy.sqlite
  )
  add_test(
    NAME multi-tu-file-registry
    COMMAND facts-tool
            --facts-out ${CMAKE_CURRENT_BINARY_DIR}/multi-tu-test.sqlite
            --files-out ${CMAKE_CURRENT_BINARY_DIR}/multi-tu-files.sqlite
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/tu_one.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/tu_two.cpp
            -- -std=c++23 -I${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures
  )
  set_tests_properties(multi-tu-file-registry PROPERTIES
    PASS_REGULAR_EXPRESSION "recorded from 3 file\\(s\\)"
  )

  add_test(
    NAME facts-tool-e2e
    COMMAND
      ${Python3_EXECUTABLE} -m pytest
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/e2e/test_facts_tool_features.py
      --verbose
      --facts-tool $<TARGET_FILE:facts-tool>
      --fixture-root ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/e2e
      --compiler ${CMAKE_CXX_COMPILER}
      --output-root ${CMAKE_CURRENT_BINARY_DIR}/e2e-artifacts
  )
  set_tests_properties(facts-tool-e2e PROPERTIES TIMEOUT 120)
endif()
