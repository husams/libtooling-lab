#include "tooling/CompilationFiles.h"

#include <clang/Tooling/CompilationDatabase.h>

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <iterator>
#include <string>
#include <vector>

namespace {

class FullCompilationDatabase final
    : public clang::tooling::CompilationDatabase {
public:
  explicit FullCompilationDatabase(
      std::vector<clang::tooling::CompileCommand> commands)
      : commands_(std::move(commands)) {}

  std::vector<clang::tooling::CompileCommand>
  getCompileCommands(llvm::StringRef file) const override {
    std::vector<clang::tooling::CompileCommand> matching;
    std::ranges::copy_if(
        commands_, std::back_inserter(matching),
        [file](const auto &command) { return command.Filename == file; });
    return matching;
  }

  std::vector<clang::tooling::CompileCommand>
  getAllCompileCommands() const override {
    return commands_;
  }

private:
  std::vector<clang::tooling::CompileCommand> commands_;
};

} // namespace

int main(int argc, char **argv) {
  assert(argc == 3);
  const auto source = std::filesystem::canonical(argv[1]);
  const auto include = std::filesystem::canonical(argv[2]);
  const auto header = std::filesystem::canonical(include / "shared.h");
  const auto otherSource = std::filesystem::canonical(include / "tu_two.cpp");
  const std::vector<std::string> arguments = {"clang++", "-std=c++23", "-I",
                                              include.string()};
  FullCompilationDatabase compilations({
      {source.parent_path().string(), source.string(), arguments, ""},
      {source.parent_path().string(), otherSource.string(), arguments, ""},
  });
  const std::vector<std::string> selectedSources = {source.string()};

  const auto discovered =
      facts::discoverCompilationFiles(compilations, selectedSources);
  assert(discovered);
  assert(std::ranges::binary_search(*discovered, source.string()));
  assert(std::ranges::binary_search(*discovered, otherSource.string()));
  assert(std::ranges::binary_search(*discovered, header.string()));
}
