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

include(FetchContent)

FetchContent_Declare(
  CLI11
  GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
  GIT_TAG 37bb6edc5317e99af72ef48405e65d9ca5218861
)
FetchContent_MakeAvailable(CLI11)

FetchContent_Declare(
  itlib
  GIT_REPOSITORY https://github.com/iboB/itlib.git
  # itlib v1.12.2
  GIT_TAG 8ed6d2efb56911ea499e7cf97335722248491639
)
FetchContent_MakeAvailable(itlib)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
               Debug Release RelWithDebInfo MinSizeRel)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)
find_package(SQLite3 REQUIRED)
# CMake >= 4.3 names the imported target SQLite3::SQLite3; older CMake (RHEL 9
# ships 3.31) only defines the now-deprecated SQLite::SQLite3.
if(NOT TARGET SQLite3::SQLite3)
  add_library(SQLite3::SQLite3 ALIAS SQLite::SQLite3)
endif()
message(STATUS "Building against LLVM ${LLVM_PACKAGE_VERSION} at ${LLVM_DIR}")

# LLVM and Clang are implementation dependencies shared by the internal modules.
add_library(facts-clang INTERFACE)
target_include_directories(facts-clang SYSTEM INTERFACE
  ${LLVM_INCLUDE_DIRS}
  ${CLANG_INCLUDE_DIRS}
)
# LLVM_DEFINITIONS is one space-separated string; split it so each -D reaches
# the compiler as its own argument.
separate_arguments(FACTS_LLVM_DEFINITIONS NATIVE_COMMAND "${LLVM_DEFINITIONS}")
target_compile_options(facts-clang INTERFACE ${FACTS_LLVM_DEFINITIONS})
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
  src/storage/Enumerator.cpp
  src/storage/FactStore.cpp
  src/storage/Function.cpp
  src/storage/FunctionInstance.cpp
  src/storage/FunctionTemplate.cpp
  src/storage/Initializer.cpp
  src/storage/Parameter.cpp
  src/storage/ProjectConfiguration.cpp
  src/storage/Record.cpp
  src/storage/RecordInstance.cpp
  src/storage/RecordTemplate.cpp
  src/storage/Relation.cpp
  src/storage/SchemaMigration.cpp
  src/storage/Sqlite.cpp
  src/storage/Storage.cpp
  src/storage/Symbol.cpp
  src/storage/Variable.cpp
)
target_include_directories(facts-storage
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
  PRIVATE ${itlib_SOURCE_DIR}/include
)
target_link_libraries(facts-storage
  PUBLIC facts-model
  PRIVATE facts-clang SQLite3::SQLite3
)

add_library(facts-tooling STATIC
  src/tooling/CompilationFiles.cpp
  src/tooling/StoredCompilationDatabase.cpp
)
target_include_directories(facts-tooling PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(facts-tooling PRIVATE facts-storage facts-clang
                                            SQLite3::SQLite3)

add_library(facts-ast STATIC
  src/ast/FactExtractor.cpp
  src/ast/extractors/Definition.cpp
  src/ast/extractors/EnumConstantDecl.cpp
  src/ast/extractors/EnumDecl.cpp
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
  src/ast/extractors/TargetResolution.cpp
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

# Commands own application behavior and remain independent of CLI11.
add_library(facts-commands STATIC
  src/commands/Extract.cpp
  src/commands/Import.cpp
)
target_include_directories(facts-commands PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(facts-commands PRIVATE
  facts-ast
  facts-storage
  facts-platform
  facts-tooling
  facts-clang
)

# CLI parsing is isolated so command behavior can be reused without parser state.
add_library(facts-cli STATIC src/cli/CommandLine.cpp)
target_include_directories(facts-cli PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(facts-cli PRIVATE facts-commands CLI11::CLI11)

# Keep the executable as the composition root; application logic lives in modules.
add_executable(facts-tool src/main.cpp)
target_link_libraries(facts-tool PRIVATE facts-cli)

add_executable(sqlite-generator-demo examples/sqlite_generator_demo.cpp)
target_link_libraries(sqlite-generator-demo PRIVATE facts-storage
                                                    SQLite3::SQLite3)

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
  add_executable(stored-compilation-database-test
                 tests/StoredCompilationDatabaseTest.cpp)
  target_link_libraries(stored-compilation-database-test
                        PRIVATE facts-tooling facts-storage facts-clang
                                SQLite3::SQLite3)
  add_test(
    NAME stored-compilation-database
    COMMAND stored-compilation-database-test
            ${CMAKE_CURRENT_BINARY_DIR}/stored-compilation-database.sqlite
  )
  add_executable(project-configuration-test tests/ProjectConfigurationTest.cpp)
  target_link_libraries(project-configuration-test
                        PRIVATE facts-tooling facts-storage facts-clang
                                SQLite3::SQLite3)
  add_test(
    NAME project-configuration
    COMMAND project-configuration-test
            ${CMAKE_CURRENT_BINARY_DIR}/project-configuration.sqlite
            ${CMAKE_CURRENT_SOURCE_DIR}
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
  add_executable(sqlite-query-test tests/SqliteQueryTest.cpp)
  target_link_libraries(sqlite-query-test PRIVATE facts-storage
                                                  SQLite3::SQLite3)
  add_test(NAME sqlite-query COMMAND sqlite-query-test)
  add_executable(sqlite-command-test tests/SqliteCommandTest.cpp)
  target_link_libraries(sqlite-command-test PRIVATE facts-storage
                                                    SQLite3::SQLite3)
  add_test(NAME sqlite-command COMMAND sqlite-command-test)
  add_test(NAME sqlite-generator-demo COMMAND sqlite-generator-demo)
  add_test(
    NAME multi-tu-project-import
    COMMAND facts-tool import
            --conf ${CMAKE_CURRENT_BINARY_DIR}/multi-tu-files.sqlite
            --extra-arg=-std=c++23
            --extra-arg=-I${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/tu_one.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/tu_two.cpp
  )
  set_tests_properties(multi-tu-project-import PROPERTIES
    FIXTURES_SETUP multi-tu-project
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  )
  add_test(
    NAME multi-tu-file-registry
    COMMAND facts-tool extract
            --output ${CMAKE_CURRENT_BINARY_DIR}/multi-tu-test.sqlite
            --conf ${CMAKE_CURRENT_BINARY_DIR}/multi-tu-files.sqlite
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/tu_one.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/tu_two.cpp
  )
  set_tests_properties(multi-tu-file-registry PROPERTIES
    FIXTURES_REQUIRED multi-tu-project
    PASS_REGULAR_EXPRESSION "recorded from 3 file\\(s\\)"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  )
  add_test(
    NAME multi-tu-project-cleanup
    COMMAND ${CMAKE_COMMAND} -E rm -f
            ${CMAKE_CURRENT_BINARY_DIR}/multi-tu-files.sqlite
            ${CMAKE_CURRENT_BINARY_DIR}/multi-tu-test.sqlite
  )
  set_tests_properties(multi-tu-project-cleanup PROPERTIES
    FIXTURES_CLEANUP multi-tu-project
  )

  add_test(
    NAME facts-tool-cli-contract
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_contract_test.py
            $<TARGET_FILE:facts-tool>
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
  # The suite parses every fixture once per scenario; a slower box needs well
# over the 120s this used to allow.
set_tests_properties(facts-tool-e2e PROPERTIES TIMEOUT 600)
endif()
