#include "platform/ResourceDirectory.h"

#if __has_include("clang/Options/OptionUtils.h")
#include "clang/Options/OptionUtils.h"
#else
#include "clang/Driver/Driver.h"
#endif
#include "clang/Basic/Version.h"

#include <dlfcn.h>

#include <algorithm>
#include <sstream>

namespace facts::platform {
namespace {

bool containsBuiltinHeaders(const std::filesystem::path &directory) {
  return std::filesystem::is_regular_file(directory / "include" / "stddef.h") &&
         std::filesystem::is_regular_file(directory / "include" / "stdarg.h");
}

std::string
attemptedCandidates(unsigned clangMajor,
                    const std::vector<std::filesystem::path> &candidates) {
  std::ostringstream message;
  message << "cannot resolve Clang " << clangMajor
          << " resource directory; attempted:";
  for (const auto &candidate : candidates)
    message << "\n  " << candidate.string();
  return message.str();
}

} // namespace

std::vector<std::filesystem::path>
resourceDirectoryCandidates(const ResourceDirectoryRequest &request) {
  const auto libraryDirectory = request.library.parent_path();
  const auto version = std::to_string(request.clangMajor);
  std::vector<std::filesystem::path> candidates{
      request.computed.lexically_normal(),
      (libraryDirectory / "clang" / version).lexically_normal(),
      (libraryDirectory / ".." / "lib" / "clang" / version).lexically_normal(),
      (libraryDirectory / ".." / "lib64" / "clang" / version)
          .lexically_normal(),
  };
  std::vector<std::filesystem::path> unique;
  for (const auto &candidate : candidates) {
    if (std::ranges::find(unique, candidate) == unique.end())
      unique.push_back(candidate);
  }
  return unique;
}

std::expected<std::filesystem::path, std::string>
resolveResourceDirectory(const ResourceDirectoryRequest &request) {
  auto candidates = resourceDirectoryCandidates(request);
  const auto selected =
      std::ranges::find_if(candidates, containsBuiltinHeaders);
  if (selected == candidates.end())
    return std::unexpected(attemptedCandidates(request.clangMajor, candidates));
  return *selected;
}

std::expected<std::filesystem::path, std::string>
resolveLinkedResourceDirectory() {
#if __has_include("clang/Options/OptionUtils.h")
  auto *resourcePath =
      static_cast<std::string (*)(llvm::StringRef)>(&clang::GetResourcesPath);
#else
  auto *resourcePath = &clang::driver::Driver::GetResourcesPath;
#endif
  const std::filesystem::path configuredLibrary(FACTS_LINKED_CLANG_CPP);
  Dl_info loadedLibrary{};
  const auto loadedLibraryPath =
      dladdr(reinterpret_cast<void *>(resourcePath), &loadedLibrary) &&
              loadedLibrary.dli_fname != nullptr
          ? std::filesystem::path(loadedLibrary.dli_fname)
          : configuredLibrary;
  const auto libraryPath = std::filesystem::exists(configuredLibrary)
                               ? configuredLibrary
                               : loadedLibraryPath;
  return resolveResourceDirectory(ResourceDirectoryRequest{
      libraryPath, resourcePath(libraryPath.string()), CLANG_VERSION_MAJOR});
}

} // namespace facts::platform
