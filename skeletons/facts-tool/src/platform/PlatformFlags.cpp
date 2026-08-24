#include "platform/PlatformFlags.h"

// GetResourcesPath moved into the clangOptions library in LLVM 22; before that
// it was a static member of the driver.
#if __has_include("clang/Options/OptionUtils.h")
#include "clang/Options/OptionUtils.h"
#else
#include "clang/Driver/Driver.h"
#endif
#include "clang/Basic/Version.h"
#include "clang/Tooling/ArgumentsAdjusters.h"
#include "clang/Tooling/Tooling.h"

#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"

#include <dlfcn.h>
#include <string>

using namespace clang;
using namespace clang::tooling;

namespace facts {
namespace {

// A resource directory is only useful if it actually holds the builtin headers
// (stddef.h, stdarg.h) that libc and libstdc++ headers include.
bool holdsBuiltinHeaders(llvm::StringRef directory) {
  if (directory.empty())
    return false;
  llvm::SmallString<128> probe(directory);
  llvm::sys::path::append(probe, "include", "stddef.h");
  return llvm::sys::fs::exists(probe);
}

std::string siblingResourceDir(llvm::StringRef libraryDir,
                               llvm::StringRef libName) {
  llvm::SmallString<128> candidate(libraryDir);
  if (!libName.empty())
    llvm::sys::path::append(candidate, "..", libName);
  llvm::sys::path::append(candidate, "clang",
                          std::to_string(CLANG_VERSION_MAJOR));
  return std::string(candidate);
}

// Distributions disagree on where the builtin headers sit relative to
// libclang-cpp: Homebrew keeps lib/clang/<major> next to the library, while
// RHEL's llvm21 package splits lib64/ and lib/. Probe the layouts instead of
// trusting the computed default, which silently yields a directory with no
// headers in it and only fails later, deep inside a system header.
std::string resolveResourceDir(llvm::StringRef libraryPath,
                               std::string computed) {
  if (holdsBuiltinHeaders(computed))
    return computed;
  llvm::StringRef dir = llvm::sys::path::parent_path(libraryPath);
  for (llvm::StringRef libName : {"", "lib", "lib64"}) {
    std::string candidate = siblingResourceDir(dir, libName);
    if (holdsBuiltinHeaders(candidate))
      return candidate;
  }
  return computed;
}

// The builtin-header dir comes from the LLVM install this binary is linked
// against — dladdr finds its libclang-cpp.dylib.
void addResourceDir(ClangTool &tool) {
#if __has_include("clang/Options/OptionUtils.h")
  auto *resourcePathFn =
      static_cast<std::string (*)(llvm::StringRef)>(&clang::GetResourcesPath);
#else
  auto *resourcePathFn = &clang::driver::Driver::GetResourcesPath;
#endif
  Dl_info info;
  if (!dladdr((void *)resourcePathFn, &info) || !info.dli_fname)
    return;
  std::string resourceDir =
      resolveResourceDir(info.dli_fname, resourcePathFn(info.dli_fname));
  tool.appendArgumentsAdjuster(getInsertArgumentAdjuster(
      {"-resource-dir", resourceDir}, ArgumentInsertPosition::END));
}

// The SDK comes from xcrun.
void addSysroot(ClangTool &tool) {
  FILE *p = popen("xcrun --show-sdk-path 2>/dev/null", "r");
  if (!p)
    return;
  char buf[256] = {};
  if (fgets(buf, sizeof(buf), p) && buf[0] == '/') {
    std::string sdk(buf);
    sdk.erase(sdk.find_last_not_of('\n') + 1);
    tool.appendArgumentsAdjuster(getInsertArgumentAdjuster(
        {"-isysroot", sdk}, ArgumentInsertPosition::END));
  }
  pclose(p);
}

} // namespace

void addPlatformFlags(ClangTool &tool) {
  addResourceDir(tool);
  addSysroot(tool);
}

} // namespace facts
