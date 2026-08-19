#include "platform/PlatformFlags.h"

// GetResourcesPath moved into the clangOptions library in LLVM 22; before that
// it was a static member of the driver.
#if __has_include("clang/Options/OptionUtils.h")
#include "clang/Options/OptionUtils.h"
#else
#include "clang/Driver/Driver.h"
#endif
#include "clang/Tooling/ArgumentsAdjusters.h"
#include "clang/Tooling/Tooling.h"

#include <dlfcn.h>
#include <string>

using namespace clang;
using namespace clang::tooling;

namespace facts {
namespace {

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
  if (dladdr((void *)resourcePathFn, &info))
    tool.appendArgumentsAdjuster(getInsertArgumentAdjuster(
        {"-resource-dir", resourcePathFn(info.dli_fname)},
        ArgumentInsertPosition::END));
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
