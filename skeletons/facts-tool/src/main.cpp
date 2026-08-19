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
#include "platform/PlatformFlags.h"
#include "storage/FactStore.h"
#include "storage/FileManager.h"
#include "tooling/CompilationFiles.h"
#include "tooling/StoredCompilationDatabase.h"

#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include <filesystem>
#include <string_view>
#include <vector>

static llvm::cl::OptionCategory toolCategory("facts-tool options");

// Names here share a namespace with every option inside libLLVM, so keep them
// specific enough not to collide.
static llvm::cl::opt<std::string>
    factsOut("facts-out", llvm::cl::desc("SQLite database for extracted facts"),
             llvm::cl::value_desc("file"), llvm::cl::init("facts.db"),
             llvm::cl::cat(toolCategory));

static llvm::cl::opt<std::string>
    filesOut("files-out", llvm::cl::desc("SQLite file identity registry"),
             llvm::cl::value_desc("file"), llvm::cl::init("files.db"),
             llvm::cl::cat(toolCategory));

namespace {

std::string storedDatabaseArgument(int argc, const char **argv) {
  constexpr std::string_view option = "--files-out";
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
  return "files.db";
}

} // namespace

int main(int argc, const char **argv) {
  facts::configureStoredCompilationDatabase(storedDatabaseArgument(argc, argv));
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
  const auto filesPath =
      std::filesystem::absolute(filesOut.getValue()).lexically_normal();
  if (factsPath == filesPath) {
    llvm::errs() << "facts-tool: facts and files require separate databases\n";
    return 1;
  }

  facts::FileManager files(filesOut);
  const auto importFiles = [&files](std::vector<std::string> paths) {
    return files.addBulk(paths);
  };
  auto imported = facts::discoverCompilationFiles(options->getCompilations(),
                                                  options->getSourcePathList())
                      .and_then(importFiles);
  if (!imported) {
    llvm::errs() << "facts-tool: cannot pre-import files: "
                 << imported.error().message() << '\n';
    return 1;
  }

  clang::tooling::ClangTool tool(options->getCompilations(),
                                 options->getSourcePathList());
  facts::addPlatformFlags(tool);

  facts::FactStore store(factsOut);
  store.begin();
  int result = tool.run(facts::createFactExtractorFactory(files, store).get());
  store.end();
  return result;
}
