#include "tooling/astcache/RevisionObserver.h"

#include "tooling/astcache/FileIdentity.h"
#include "tooling/astcache/Revisions.h"
#include "tooling/astcache/SearchDirectories.h"

#include <clang/Basic/SourceManager.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Lex/PPCallbacks.h>
#include <clang/Lex/Preprocessor.h>

namespace facts::astcache::detail {
namespace {

void observeDirectory(const fs::path &directory,
                      RevisionObservations &observations) {
  auto [position, inserted] = observations.directories.try_emplace(directory);
  if (inserted)
    position->second = readRepositoryRevision(directory, true);
  const auto &revision = position->second;
  if (!revision)
    return;
  const auto [saved, first] =
      observations.repositories.try_emplace(revision->path, *revision);
  if (!first && saved->second != *revision)
    observations.error = "Git commit changed while preprocessing " + revision->path;
}

void observeFile(clang::FileEntryRef file, const fs::path &workingDirectory,
                 RevisionObservations &observations) {
  const auto real = file.getFileEntry().tryGetRealPathName();
  const auto spelling = real.empty() ? file.getName() : real;
  observeDirectory(resolve(spelling.str(), workingDirectory).parent_path(), observations);
}

class RevisionObserver final : public clang::PPCallbacks {
public:
  RevisionObserver(clang::SourceManager &manager, fs::path workingDirectory,
                   RevisionObservations &observations)
      : manager_(manager), workingDirectory_(std::move(workingDirectory)),
        observations_(observations) {}

  void FileChanged(clang::SourceLocation location, FileChangeReason reason,
                   clang::SrcMgr::CharacteristicKind, clang::FileID) override {
    if (reason != EnterFile)
      return;
    if (const auto file = manager_.getFileEntryRefForID(manager_.getFileID(location)))
      observeFile(*file, workingDirectory_, observations_);
  }

  void InclusionDirective(clang::SourceLocation, const clang::Token &,
                          llvm::StringRef, bool, clang::CharSourceRange,
                          clang::OptionalFileEntryRef file, llvm::StringRef,
                          llvm::StringRef, const clang::Module *, bool,
                          clang::SrcMgr::CharacteristicKind) override {
    if (file)
      observeFile(*file, workingDirectory_, observations_);
  }

private:
  clang::SourceManager &manager_;
  fs::path workingDirectory_;
  RevisionObservations &observations_;
};

} // namespace

void attachRevisionObserver(clang::CompilerInstance &compiler,
                            const fs::path &workingDirectory,
                            RevisionObservations &observations) {
  auto &manager = compiler.getSourceManager();
  if (const auto main = manager.getFileEntryRefForID(manager.getMainFileID()))
    observeFile(*main, workingDirectory, observations);
  for (const auto &directory :
       headerSearchDirectories(compiler.getPreprocessor(), workingDirectory))
    observeDirectory(directory, observations);
  compiler.getPreprocessor().addPPCallbacks(std::make_unique<RevisionObserver>(
      manager, workingDirectory, observations));
}

std::expected<void, std::string>
validateObservedRevisions(const RevisionObservations &observations) {
  if (observations.error)
    return std::unexpected(*observations.error);
  for (const auto &[path, saved] : observations.repositories) {
    const auto current = readRepositoryRevision(path, false);
    if (!current || *current != saved)
      return std::unexpected("Git commit changed while preprocessing " + path);
  }
  return {};
}

} // namespace facts::astcache::detail
