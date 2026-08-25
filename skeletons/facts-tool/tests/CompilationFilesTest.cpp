#undef NDEBUG

#include "tooling/CompilationFiles.h"

#include <clang/Tooling/CompilationDatabase.h>

#include <algorithm>
#include <cassert>
#include <expected>
#include <filesystem>
#include <iostream>
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

std::filesystem::path resetScratchDirectory(const std::filesystem::path &path) {
  std::error_code error;
  std::filesystem::permissions(path / "denied",
                               std::filesystem::perms::owner_all,
                               std::filesystem::perm_options::replace, error);
  std::filesystem::remove_all(path, error);
  std::filesystem::create_directories(path);
  return std::filesystem::canonical(path);
}

std::expected<facts::CompilationFiles, std::string>
discoverWithIncludeRoot(const std::filesystem::path &source,
                        const std::filesystem::path &otherSource,
                        const std::filesystem::path &includeRoot) {
  const std::vector<std::string> arguments = {"clang++", "-std=c++23", "-I",
                                              includeRoot.string()};
  FullCompilationDatabase compilations({
      {source.parent_path().string(), source.string(), arguments, ""},
      {source.parent_path().string(), otherSource.string(), arguments, ""},
  });
  const std::vector<std::string> selectedSources = {source.string()};
  return facts::discoverCompilationFiles(compilations, selectedSources);
}

} // namespace

int main(int argc, char **argv) {
  assert(argc == 4);
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
  assert(std::ranges::binary_search(discovered->files, source.string()));
  assert(std::ranges::binary_search(discovered->files, otherSource.string()));
  assert(std::ranges::binary_search(discovered->files, header.string()));
  assert(discovered->diagnostics.empty());

  const auto missingInclude = include / "missing-include-root";
  assert(!std::filesystem::exists(missingInclude));
  const std::vector<std::string> unrelatedArguments = {
      "clang++",        "-std=c++23", "-I",
      include.string(), "-I",         missingInclude.string()};
  FullCompilationDatabase withMissingIncludeRoot({
      {source.parent_path().string(), source.string(), arguments, ""},
      {source.parent_path().string(), otherSource.string(), unrelatedArguments,
       ""},
  });

  const auto unaffected =
      facts::discoverCompilationFiles(withMissingIncludeRoot, selectedSources);
  assert(unaffected);
  assert(std::ranges::binary_search(unaffected->files, source.string()));
  assert(std::ranges::binary_search(unaffected->files, header.string()));
  assert(unaffected->diagnostics.empty());

  const std::vector<std::string> unrelatedSelection = {otherSource.string()};
  const auto skipped = facts::discoverCompilationFiles(withMissingIncludeRoot,
                                                       unrelatedSelection);
  assert(skipped);
  assert(std::ranges::binary_search(skipped->files, otherSource.string()));
  assert(skipped->diagnostics.size() == 1);
  assert(skipped->diagnostics.front().starts_with(
      "skipping unavailable include directory '" + missingInclude.string() +
      "'"));

  const std::vector<std::string> missingSelection = {
      (include / "absent-source.cpp").string()};
  const auto absent =
      facts::discoverCompilationFiles(compilations, missingSelection);
  assert(!absent);
  assert(absent.error().starts_with("cannot resolve source file '" +
                                    missingSelection.front() + "'"));

  const auto scratch = resetScratchDirectory(argv[3]);

  // An include root that exists but cannot be resolved to a directory stays
  // fatal. A self-referential symlink is the portable, privilege-independent
  // way to produce that state.
  const auto loopRoot = scratch / "loop";
  std::filesystem::create_symlink(loopRoot, loopRoot);
  assert(
      std::filesystem::is_symlink(std::filesystem::symlink_status(loopRoot)));
  const auto loopFailure =
      discoverWithIncludeRoot(source, otherSource, loopRoot);
  assert(!loopFailure);
  assert(loopFailure.error().starts_with("cannot inspect include directory '" +
                                         loopRoot.string() + "'"));

  // An include root that exists and is a directory but cannot be traversed
  // stays fatal too. Skipped when the running user can traverse it anyway,
  // which is the case for a privileged process.
  const auto deniedRoot = scratch / "denied";
  std::filesystem::create_directory(deniedRoot);
  std::filesystem::permissions(deniedRoot, std::filesystem::perms::none,
                               std::filesystem::perm_options::replace);
  std::error_code deniedProbe;
  std::filesystem::directory_iterator(deniedRoot, deniedProbe);
  if (deniedProbe) {
    assert(std::filesystem::is_directory(deniedRoot));
    const auto deniedFailure =
        discoverWithIncludeRoot(source, otherSource, deniedRoot);
    assert(!deniedFailure);
    assert(deniedFailure.error().starts_with("cannot scan include directory '" +
                                             deniedRoot.string() + "'"));
  } else {
    std::cerr << "compilation-files-test: skipping the permission-denied root "
                 "check; this process can traverse an unreadable directory\n";
  }
  std::filesystem::permissions(deniedRoot, std::filesystem::perms::owner_all,
                               std::filesystem::perm_options::replace);
  std::filesystem::remove_all(scratch);
}
