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

FetchContent_Declare(
  ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
  GIT_TAG v7.0.1
)
FetchContent_MakeAvailable(ftxui)

set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
set(YAML_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  yaml_cpp
  URL https://github.com/jbeder/yaml-cpp/releases/download/yaml-cpp-0.9.0/yaml-cpp-yaml-cpp-0.9.0.tar.gz
  URL_HASH SHA256=298593d9c440fd9034b8b193d96318b76d49bc97c6ceadb7b0836edf0b6d7539
)
FetchContent_MakeAvailable(yaml_cpp)

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
# SQLite is compiled from the official amalgamation and linked statically, so
# every platform gets the same version regardless of what the system ships.
# RHEL 9's 3.34 predates RETURNING, which the storage layer uses; macOS's SDK
# copy is a dynamic library outside our control. Set FACTS_SYSTEM_SQLITE=ON to
# link the system library instead (it must be >= 3.35).
option(FACTS_SYSTEM_SQLITE "Link the system SQLite instead of the bundled amalgamation" OFF)

if(FACTS_SYSTEM_SQLITE)
  find_package(SQLite3 3.35 REQUIRED)
  # One target for SQLite, whatever find_package produced. CMake >= 4.3 names
  # the imported target SQLite3::SQLite3; older CMake (RHEL 9 ships 3.31) only
  # defines the now-deprecated SQLite::SQLite3, and a hand-pointed
  # SQLite3_LIBRARY may yield no imported target at all.
  add_library(facts-sqlite INTERFACE)
  if(TARGET SQLite3::SQLite3)
    target_link_libraries(facts-sqlite INTERFACE SQLite3::SQLite3)
  elseif(TARGET SQLite::SQLite3)
    target_link_libraries(facts-sqlite INTERFACE SQLite::SQLite3)
  else()
    target_include_directories(facts-sqlite SYSTEM INTERFACE
                               ${SQLite3_INCLUDE_DIRS})
    target_link_libraries(facts-sqlite INTERFACE ${SQLite3_LIBRARIES})
  endif()
  # A hand-built libsqlite3.a carries no dependencies of its own; the threading,
  # dlopen and math symbols it references have to come from the link line.
  get_filename_component(FACTS_SQLITE_SUFFIX "${SQLite3_LIBRARY}" LAST_EXT)
  if(FACTS_SQLITE_SUFFIX STREQUAL "${CMAKE_STATIC_LIBRARY_SUFFIX}")
    find_package(Threads REQUIRED)
    target_link_libraries(facts-sqlite INTERFACE Threads::Threads
                                                 ${CMAKE_DL_LIBS} m)
  endif()
else()
  # The zip carries no CMakeLists.txt, so MakeAvailable only unpacks it. Point
  # FETCHCONTENT_SOURCE_DIR_SQLITE3 at an unpacked amalgamation to build offline.
  FetchContent_Declare(
    sqlite3
    URL https://www.sqlite.org/2026/sqlite-amalgamation-3530400.zip
    URL_HASH SHA256=1e71ddf93849c6a6ecf58b827c0692073d2dd7ee40196158068f7b29f422e87d
  )
  FetchContent_MakeAvailable(sqlite3)

  add_library(facts-sqlite STATIC ${sqlite3_SOURCE_DIR}/sqlite3.c)
  # SYSTEM so sqlite3.h never draws warnings from our own flags, PUBLIC because
  # the storage headers include <sqlite3.h>.
  target_include_directories(facts-sqlite SYSTEM PUBLIC ${sqlite3_SOURCE_DIR})
  # SQLITE_USE_URI keeps URI filenames working the way macOS's system SQLite
  # does, so a database path is interpreted identically on both platforms.
  target_compile_definitions(facts-sqlite PRIVATE
    SQLITE_ENABLE_FTS5
    SQLITE_ENABLE_JSON1
    SQLITE_ENABLE_RTREE
    SQLITE_USE_URI=1
    SQLITE_OMIT_LOAD_EXTENSION=1
    SQLITE_THREADSAFE=1
  )
  set_target_properties(facts-sqlite PROPERTIES POSITION_INDEPENDENT_CODE ON)
  find_package(Threads REQUIRED)
  target_link_libraries(facts-sqlite PUBLIC Threads::Threads)
  if(UNIX)
    target_link_libraries(facts-sqlite PUBLIC m)
  endif()
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
  src/storage/catalog/Database.cpp
  src/storage/catalog/Repository.cpp
  src/storage/catalog/Component.cpp
  src/storage/catalog/Directory.cpp
  src/storage/catalog/Clone.cpp
  src/storage/catalog/Registration.cpp
  src/storage/catalog/Removal.cpp
  src/storage/catalog/File.cpp
  src/storage/DependencyDatabase.cpp
  src/storage/FileDatabase.cpp
  src/storage/FileIdentity.cpp
  src/storage/FileManager.cpp
  src/storage/FilePersistence.cpp
  src/storage/FileSchemaMigration.cpp
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
  src/storage/RelationSite.cpp
  src/analysis/callgraph/RelationSiteContextStore.cpp
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
# PUBLIC: storage's own headers include <sqlite3.h>, so every target that
# includes them needs the SQLite include path too.
target_link_libraries(facts-storage
  PUBLIC facts-model facts-sqlite
  PRIVATE facts-clang
)

add_library(facts-tooling STATIC
  src/tooling/CompilationFiles.cpp
  src/tooling/CompilationCommandCodec.cpp
  src/tooling/ProjectImport.cpp
  src/tooling/StoredCompilationDatabase.cpp
  src/tooling/StoredCompilationReader.cpp
)
target_include_directories(facts-tooling PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(facts-tooling PRIVATE facts-storage facts-clang
                                            facts-sqlite)

add_library(facts-config STATIC
  src/config/ConfigurationPaths.cpp
  src/config/ConfigurationSearch.cpp
  src/config/ConfigurationMerge.cpp
  src/config/ConfigurationYaml.cpp
  src/config/ConfigurationOwnership.cpp
)
target_include_directories(facts-config PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(facts-config PUBLIC yaml-cpp::yaml-cpp facts-sqlite)

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
  src/ast/extractors/Reference.cpp
  src/ast/extractors/CallSite.cpp
  src/ast/extractors/ReceiverContext.cpp
  src/ast/extractors/OverrideRelation.cpp
  src/ast/extractors/RelationTarget.cpp
  src/ast/extractors/TargetResolution.cpp
  src/ast/extractors/TemplatePattern.cpp
  src/ast/extractors/TemplateSpecialization.cpp
  src/ast/extractors/Type.cpp
  src/ast/extractors/ValueDecl.cpp
  src/ast/extractors/VarDecl.cpp
  src/ast/visitors/SymbolCollector.cpp
  src/ast/visitors/IncludeVisitor.cpp
  src/ast/visitors/BodyVisitor.cpp
  src/ast/visitors/CallGraphVisitor.cpp
  src/ast/visitors/SymbolVisitor.cpp
  src/ast/visitors/Traversal.cpp
  src/analysis/callgraph/CallGraphTypes.cpp
  src/analysis/callgraph/CallGraphLinker.cpp
  src/analysis/callgraph/OverrideIndex.cpp
  src/analysis/callgraph/ReceiverPropagation.cpp
  src/analysis/callgraph/DispatchResolver.cpp
  src/analysis/callgraph/CallGraphContext.cpp
  src/analysis/callgraph/CallGraphQuery.cpp
  src/analysis/callgraph/CallGraphTraversal.cpp
)
target_include_directories(facts-ast PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(facts-ast
  PUBLIC facts-model
  PRIVATE facts-storage facts-clang
)

add_library(facts-platform STATIC
  src/platform/DriverIncludes.cpp
  src/platform/PlatformFlags.cpp
  src/platform/ResourceDirectory.cpp
)
target_include_directories(facts-platform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(
  facts-platform PRIVATE FACTS_LINKED_CLANG_CPP="$<TARGET_FILE:clang-cpp>"
)
target_link_libraries(facts-platform PRIVATE facts-clang)

add_library(facts-symbols STATIC
  src/commands/catalog/SymbolFormat.cpp
  src/commands/catalog/SymbolQuery.cpp
)
target_include_directories(facts-symbols PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
# PUBLIC storage: SymbolData.h includes storage/catalog/Database.h, which pulls
# in <sqlite3.h>, so consumers (facts-symbol-ui) need the SQLite include path.
target_link_libraries(facts-symbols PUBLIC facts-storage PRIVATE facts-clang)

add_library(facts-symbol-ui STATIC
  src/ui/symbol/SymbolBrowser.cpp
  src/ui/symbol/SymbolDetail.cpp
  src/ui/symbol/VirtualList.cpp
  src/ui/symbol/VirtualListModel.cpp
)
target_include_directories(facts-symbol-ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(facts-symbol-ui
  PUBLIC facts-symbols
  PRIVATE ftxui::component ftxui::dom ftxui::screen
)

# Commands own application behavior and remain independent of CLI11.
add_library(facts-commands STATIC
  src/commands/catalog/Repository.cpp
  src/commands/catalog/Component.cpp
  src/commands/catalog/Directory.cpp
  src/commands/catalog/Display.cpp
  src/commands/catalog/Export.cpp
  src/commands/catalog/File.cpp
  src/commands/catalog/Symbol.cpp
  src/commands/DatabasePaths.cpp
  src/commands/Configuration.cpp
  src/commands/Dependency.cpp
  src/commands/Extract.cpp
  src/commands/ExtractionSetup.cpp
  src/commands/IncludedFiles.cpp
  src/commands/Import.cpp
  src/commands/Match.cpp
  src/commands/match/ArgumentOutput.cpp
  src/commands/match/DirectCall.cpp
  src/commands/match/MatchCallback.cpp
  src/commands/match/MatchContract.cpp
  src/commands/match/NearestCaller.cpp
  src/commands/match/RelationKinds.cpp
  src/commands/match/RelationPersistence.cpp
  src/commands/match/RelationValidation.cpp
  src/commands/match/SymbolDispatch.cpp
  src/commands/analyse/CallGraphCommand.cpp
)
target_include_directories(facts-commands PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(facts-commands PRIVATE
  facts-ast
  facts-storage
  facts-platform
  facts-symbols
  facts-symbol-ui
  facts-tooling
  facts-config
  facts-clang
)

# CLI parsing is isolated so command behavior can be reused without parser state.
add_library(facts-cli STATIC
  src/cli/CommandLine.cpp
  src/cli/Dispatch.cpp
  src/cli/MatchCommandLine.cpp
  src/cli/catalog/Repository.cpp
  src/cli/catalog/Component.cpp
  src/cli/catalog/Directory.cpp
  src/cli/catalog/File.cpp
  src/cli/catalog/Symbol.cpp
)
target_include_directories(facts-cli PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(facts-cli PRIVATE facts-commands CLI11::CLI11)

# Keep the executable as the composition root; application logic lives in modules.
add_executable(facts-tool src/main.cpp)
target_link_libraries(facts-tool PRIVATE facts-cli)

add_executable(sqlite-generator-demo examples/sqlite_generator_demo.cpp)
target_link_libraries(sqlite-generator-demo PRIVATE facts-storage
                                                    facts-sqlite)

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
                                              facts-sqlite)
  add_test(
    NAME fact-store
    COMMAND fact-store-test ${CMAKE_CURRENT_BINARY_DIR}/fact-store-test.sqlite
  )
  add_executable(call-graph-test tests/CallGraphTest.cpp)
  target_link_libraries(call-graph-test PRIVATE facts-ast facts-storage)
  add_test(NAME call-graph COMMAND call-graph-test)
  add_executable(file-manager-test tests/FileManagerTest.cpp)
  target_link_libraries(file-manager-test PRIVATE facts-storage facts-model
                                                facts-clang facts-sqlite)
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
            ${CMAKE_CURRENT_BINARY_DIR}/compilation-files-scratch
  )
  add_executable(stored-compilation-database-test
                 tests/StoredCompilationDatabaseTest.cpp)
  target_link_libraries(stored-compilation-database-test
                        PRIVATE facts-tooling facts-storage facts-clang
                                facts-sqlite)
  add_test(
    NAME stored-compilation-database
    COMMAND stored-compilation-database-test
            ${CMAKE_CURRENT_BINARY_DIR}/stored-compilation-database.sqlite
  )
  add_executable(stored-compilation-missing-command-test
                 tests/StoredCompilationMissingCommandTest.cpp)
  target_link_libraries(stored-compilation-missing-command-test
                        PRIVATE facts-tooling facts-storage facts-clang
                                facts-sqlite)
  add_test(
    NAME stored-compilation-missing-command
    COMMAND stored-compilation-missing-command-test
            ${CMAKE_CURRENT_BINARY_DIR}/stored-compilation-missing-command.sqlite
  )
  add_executable(platform-flags-test tests/PlatformFlagsTest.cpp)
  target_link_libraries(platform-flags-test PRIVATE facts-platform facts-clang)
  add_test(
    NAME platform-flags
    COMMAND platform-flags-test
            ${CMAKE_CURRENT_BINARY_DIR}/platform-flags-layouts
  )
  add_executable(project-configuration-test tests/ProjectConfigurationTest.cpp)
  target_link_libraries(project-configuration-test
                        PRIVATE facts-tooling facts-storage facts-clang
                                facts-sqlite)
  add_test(
    NAME project-configuration
    COMMAND project-configuration-test
            ${CMAKE_CURRENT_BINARY_DIR}/project-configuration.sqlite
            ${CMAKE_CURRENT_SOURCE_DIR}
  )
  add_executable(configuration-test tests/ConfigurationTest.cpp
    tests/configuration/Schema.cpp tests/configuration/Paths.cpp
    tests/configuration/Discovery.cpp tests/configuration/Arguments.cpp
    tests/configuration/Ownership.cpp tests/configuration/Safety.cpp
    tests/configuration/Policies.cpp
    tests/configuration/Placeholders.cpp)
  target_link_libraries(configuration-test PRIVATE facts-config facts-clang)
  add_test(NAME configuration COMMAND configuration-test)
  add_executable(
    storage-schema-test
    tests/storage/FreshSchemaTest.cpp
    tests/storage/CallGraphMigrationTest.cpp
    tests/storage/LegacySchemaMigrationTest.cpp
    tests/storage/StorageSchemaTestMain.cpp
    tests/storage/StorageSchemaTestSupport.cpp
  )
  target_link_libraries(storage-schema-test PRIVATE facts-storage facts-model
                                                    facts-sqlite)
  add_test(
    NAME storage-schema
    COMMAND storage-schema-test
            ${CMAKE_CURRENT_BINARY_DIR}/storage-schema-fresh.sqlite
            ${CMAKE_CURRENT_BINARY_DIR}/storage-schema-legacy.sqlite
  )
  add_executable(reference-extraction-test tests/ReferenceExtractionTest.cpp)
  target_link_libraries(reference-extraction-test PRIVATE
    facts-ast facts-storage facts-model facts-clang facts-sqlite)
  add_test(
    NAME reference-extraction
    COMMAND reference-extraction-test
            $<TARGET_FILE:facts-tool>
            ${CMAKE_CXX_COMPILER}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/e2e
            ${CMAKE_CURRENT_BINARY_DIR}/reference-extraction-artifacts
  )
  add_executable(type-resolution-error-test tests/TypeResolutionErrorTest.cpp)
  # StoreExtracted.h pulls in storage/FactStore.h, so this target needs the
  # SQLite include path and link line too; facts-ast links facts-storage
  # PRIVATE and does not propagate them.
  target_link_libraries(type-resolution-error-test PRIVATE
    facts-ast facts-storage facts-model facts-clang facts-sqlite)
  add_test(NAME type-resolution-error COMMAND type-resolution-error-test)
  add_executable(match-relation-contract-test
                 tests/match/RelationContractTest.cpp)
  target_link_libraries(match-relation-contract-test PRIVATE
                        facts-commands facts-clang)
  add_test(NAME match-relation-contract COMMAND match-relation-contract-test)
  add_executable(match-nearest-caller-test tests/match/NearestCallerTest.cpp)
  target_link_libraries(match-nearest-caller-test PRIVATE
                        facts-commands facts-clang)
  add_test(NAME match-nearest-caller COMMAND match-nearest-caller-test)
  add_executable(file-database-persistence-test
                 tests/FileDatabasePersistenceTest.cpp)
  target_link_libraries(file-database-persistence-test
                        PRIVATE facts-storage facts-model facts-sqlite)
  add_test(
    NAME file-database-persistence
    COMMAND file-database-persistence-test
            ${CMAKE_CURRENT_BINARY_DIR}/storage-connection.sqlite
            ${CMAKE_CURRENT_BINARY_DIR}/file-database-persistence.sqlite
  )
  add_executable(sqlite-query-test tests/SqliteQueryTest.cpp)
  target_link_libraries(sqlite-query-test PRIVATE facts-storage
                                                  facts-sqlite)
  add_test(NAME sqlite-query COMMAND sqlite-query-test)
  add_executable(sqlite-command-test tests/SqliteCommandTest.cpp)
  target_link_libraries(sqlite-command-test PRIVATE facts-storage
                                                    facts-sqlite)
  add_test(NAME sqlite-command COMMAND sqlite-command-test)
  add_executable(symbol-browser-test tests/SymbolBrowserTest.cpp)
  target_link_libraries(symbol-browser-test PRIVATE facts-symbol-ui)
  add_test(NAME symbol-browser COMMAND symbol-browser-test)
  add_executable(dependency-database-test tests/DependencyDatabaseTest.cpp)
  target_link_libraries(dependency-database-test PRIVATE facts-storage
                                                         facts-sqlite)
  add_test(NAME sqlite-generator-demo COMMAND sqlite-generator-demo)
  set(dependency_config
      ${CMAKE_CURRENT_BINARY_DIR}/dependency-analysis.sqlite)
  set(dependency_output
      ${CMAKE_CURRENT_BINARY_DIR}/dependency-analysis-output.sqlite)
  set(dependency_fixture
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/dependency)
  add_test(
    NAME dependency-analysis-import
    COMMAND facts-tool import
            --conf ${dependency_config}
            --extra-arg=-std=c++23
            --extra-arg=-I${dependency_fixture}
            ${dependency_fixture}/root.cpp
  )
  add_test(
    NAME dependency-analysis-first
    COMMAND facts-tool analyse dependency
            --output ${dependency_output}
            --conf ${dependency_config}
            ${dependency_fixture}/root.cpp
  )
  set_tests_properties(dependency-analysis-first PROPERTIES
    DEPENDS dependency-analysis-import)
  add_test(
    NAME dependency-analysis-second
    COMMAND facts-tool analyse dependency
            --output ${dependency_output}
            --conf ${dependency_config}
            ${dependency_fixture}/root.cpp
  )
  set_tests_properties(dependency-analysis-second PROPERTIES
    DEPENDS dependency-analysis-first)
  add_test(
    NAME dependency-analysis-extract
    COMMAND facts-tool extract
            --output ${dependency_output}
            --conf ${dependency_config}
            ${dependency_fixture}/root.cpp
  )
  set_tests_properties(dependency-analysis-extract PROPERTIES
    DEPENDS dependency-analysis-second)
  add_test(
    NAME dependency-analysis-verify
    COMMAND dependency-database-test
            ${dependency_output}
            ${dependency_config}
            ${dependency_fixture}/root.cpp
            ${dependency_fixture}/b.h
            ${dependency_fixture}/c.h
  )
  set_tests_properties(dependency-analysis-verify PROPERTIES
    DEPENDS dependency-analysis-extract)
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
    NAME call-graph-architecture
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/call_graph_architecture_test.py
            ${CMAKE_CURRENT_SOURCE_DIR}
  )

  add_test(
    NAME gcc-clang-ast-shim
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/gcc_clang_ast_shim_test.py
  )

  # A precompiled header is only readable by the exact Clang that wrote it, so
  # the prefix-header scenarios need the clang++ that ships beside the libClang
  # this tool links -- CMAKE_CXX_COMPILER may be GCC, whose PCH Clang rejects.
  # Where that driver is absent those scenarios skip themselves.
  set(FACTS_E2E_CLANG_DRIVER_ARGUMENT)
  if(EXISTS "${LLVM_TOOLS_BINARY_DIR}/clang++")
    set(FACTS_E2E_CLANG_DRIVER_ARGUMENT
        --clang-driver "${LLVM_TOOLS_BINARY_DIR}/clang++")
  endif()
  add_test(
    NAME facts-tool-e2e
    COMMAND
      ${Python3_EXECUTABLE} -m pytest
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/e2e/test_facts_tool_features.py
      --verbose
      --facts-tool $<TARGET_FILE:facts-tool>
      --fixture-root ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/e2e
      --compiler ${CMAKE_CXX_COMPILER}
      ${FACTS_E2E_CLANG_DRIVER_ARGUMENT}
      --output-root ${CMAKE_CURRENT_BINARY_DIR}/e2e-artifacts
  )
  # The suite parses every fixture once per scenario; a slower box needs well
# over the 120s this used to allow.
set_tests_properties(facts-tool-e2e PROPERTIES TIMEOUT 600)
  # Test assertions include required operations; execute them in release builds too.
  get_property(FACTS_LOCAL_TARGETS DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
  foreach(FACTS_TEST_TARGET IN LISTS FACTS_LOCAL_TARGETS)
    if(FACTS_TEST_TARGET MATCHES "-test$")
      target_compile_options(${FACTS_TEST_TARGET} PRIVATE -UNDEBUG)
    endif()
  endforeach()
endif()
