// facts-tool — skeleton for extracting facts from the Clang AST.
//
// The tool is split along the one boundary that matters here: ast/ reads the
// AST while it is alive, storage/ keeps what survives it. main only wires
// the two together and runs the ClangTool once per input file.
//
//   src/main.cpp          argument parsing, the ClangTool run
//   src/PlatformFlags.*   SDK / builtin-header paths for the embedded parser
//   src/ast/              the AST side — visits nodes, produces facts
//   src/storage/       the storage side — outlives the ASTContext
//
// It compiles and runs as-is and extracts nothing yet; the fact model and the
// traversal are what gets built next.
//
// Run:
//   ./build/facts-tool sample/sample.cpp -- -std=c++23

#include "ast/FactExtractor.h"
#include "ast/Indexing.h"
#include "platform/PlatformFlags.h"
#include "storage/FactStore.h"
#include "storage/FileManager.h"
#include "tooling/CompilationFiles.h"
#include "tooling/ProjectImport.h"
#include "tooling/StoredCompilationDatabase.h"

#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include <chrono>
#include <filesystem>
#include <string_view>
#include <thread>
#include <vector>

static llvm::cl::OptionCategory toolCategory("facts-tool options");

// Names here share a namespace with every option inside libLLVM, so keep them
// specific enough not to collide.
static llvm::cl::opt<std::string>
    factsOut("facts-out", llvm::cl::desc("SQLite database for extracted facts"),
             llvm::cl::value_desc("file"), llvm::cl::init("facts.db"),
             llvm::cl::cat(toolCategory));

static llvm::cl::opt<std::string> projectConfig(
    "project-config",
    llvm::cl::desc("SQLite project configuration and file registry"),
    llvm::cl::value_desc("database"), llvm::cl::init("project.db"),
    llvm::cl::cat(toolCategory));

static llvm::cl::opt<bool> refreshProjectConfig(
    "refresh-project-config",
    llvm::cl::desc("Replace persisted project configuration from the input "
                   "compilation database"),
    llvm::cl::init(false), llvm::cl::cat(toolCategory));

static llvm::cl::list<std::string> projectComponent(
    "project-component",
    llvm::cl::desc("Component as name=path, repeatable for nested projects"),
    llvm::cl::ZeroOrMore, llvm::cl::cat(toolCategory));

namespace {

std::string projectDatabaseArgument(int argc, const char **argv) {
  constexpr std::string_view option = "--project-config";
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument = argv[index];
    if (argument == option && index + 1 < argc) {
      return argv[index + 1];
    }
    if (argument.starts_with(option) && argument.size() > option.size() &&
        argument[option.size()] == '=') {
      return std::string(argument.substr(option.size() + 1));
    }
  }
  return "project.db";
}

std::expected<std::vector<facts::ProjectComponent>, std::string>
projectComponents() {
  std::vector<facts::ProjectComponent> components;
  for (const auto &specification : projectComponent) {
    const auto separator = specification.find('=');
    if (separator == std::string::npos || separator == 0 ||
        separator + 1 == specification.size()) {
      return std::unexpected("--project-component requires name=path: " +
                             specification);
    }
    components.push_back(
        facts::ProjectComponent{.name = specification.substr(0, separator),
                                .path = specification.substr(separator + 1),
                                .kind = "repo"});
  }
  return components;
}

} // namespace

int main(int argc, const char **argv) {
  facts::configureStoredCompilationDatabase(
      projectDatabaseArgument(argc, argv));
  auto options =
      clang::tooling::CommonOptionsParser::create(argc, argv, toolCategory);
  if (!options) {
    llvm::errs() << llvm::toString(options.takeError());
    return 1;
  }
  if (const auto error = facts::storedCompilationDatabaseError()) {
    llvm::errs() << "facts-tool: invalid stored compile options: " << *error
                 << '\n';
    return 1;
  }

  const auto factsPath =
      std::filesystem::absolute(factsOut.getValue()).lexically_normal();
  const auto projectPath =
      std::filesystem::absolute(projectConfig.getValue()).lexically_normal();
  if (factsPath == projectPath) {
    llvm::errs()
        << "facts-tool: facts and project config require separate databases\n";
    return 1;
  }

  facts::FileManager files(projectConfig);
  auto stored = facts::loadStoredCompilationDatabase(projectConfig);
  if (!stored && !refreshProjectConfig) {
    llvm::errs() << "facts-tool: invalid stored project configuration: "
                 << stored.error() << '\n';
    return 1;
  }
  const auto hasStoredCommands =
      stored && !(*stored)->getAllCompileCommands().empty();
  if (refreshProjectConfig || !hasStoredCommands) {
    auto components = projectComponents();
    if (!components) {
      llvm::errs() << "facts-tool: " << components.error() << '\n';
      return 1;
    }
    facts::ProjectImportOptions importOptions;
    importOptions.components = std::move(*components);
    auto imported = facts::importProjectConfiguration(
        files, options->getCompilations(), options->getSourcePathList(),
        importOptions);
    if (!imported) {
      llvm::errs() << "facts-tool: cannot import project configuration: "
                   << imported.error() << '\n';
      return 1;
    }
    for (const auto &diagnostic : imported->diagnostics) {
      llvm::errs() << "facts-tool: " << diagnostic << '\n';
    }
  }

  auto reloaded = facts::loadStoredCompilationDatabase(projectConfig);
  for (int attempt = 0;
       attempt < 20 && reloaded && (*reloaded)->getAllCompileCommands().empty();
       ++attempt) {
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    reloaded = facts::loadStoredCompilationDatabase(projectConfig);
  }
  if (!reloaded || (*reloaded)->getAllCompileCommands().empty()) {
    llvm::errs() << "facts-tool: project configuration contains no stored "
                    "compile commands\n";
    return 1;
  }
  auto sourcePaths = options->getSourcePathList();
  if (sourcePaths.empty()) {
    sourcePaths = (*reloaded)->getAllFiles();
  }
  auto imported = facts::discoverCompilationFiles(**reloaded, sourcePaths)
                      .and_then([&files](std::vector<std::string> paths) {
                        return files.addBulk(paths);
                      });
  if (!imported) {
    llvm::errs() << "facts-tool: cannot pre-import files: "
                 << imported.error().message() << '\n';
    return 1;
  }

  clang::tooling::ClangTool tool(**reloaded, sourcePaths);
  facts::addPlatformFlags(tool);

  facts::FactStore store(factsOut);
  facts::IndexingStatus indexing;
  store.begin();
  const int toolResult =
      tool.run(facts::createFactExtractorFactory(files, store, indexing).get());
  store.end();
  return toolResult != 0 ? toolResult : indexing.complete() ? 0 : 1;
}
