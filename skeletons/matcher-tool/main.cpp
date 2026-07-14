// matcher-tool — skeleton for an AST-matcher-based Clang tool.
//
// The wiring is complete and compiles as-is, with one placeholder matcher
// that fires on every function declaration. Replace it with your own.
//
// Run:
//   ./build/matcher-tool sample/sample.cpp -- -std=c++17

#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Options/OptionUtils.h"
#include "clang/Tooling/ArgumentsAdjusters.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"

#include <dlfcn.h>

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;

static llvm::cl::OptionCategory ToolCategory("matcher-tool options");

// The embedded front-end can't locate headers on its own; point it at what's
// already installed. The builtin-header dir comes from the LLVM install this
// binary is linked against (dladdr finds its libclang-cpp.dylib), the SDK
// from xcrun.
static void addPlatformFlags(ClangTool &Tool) {
  auto *ResourcePathFn =
      static_cast<std::string (*)(llvm::StringRef)>(&clang::GetResourcesPath);
  Dl_info Info;
  if (dladdr((void *)ResourcePathFn, &Info))
    Tool.appendArgumentsAdjuster(getInsertArgumentAdjuster(
        {"-resource-dir", ResourcePathFn(Info.dli_fname)},
        ArgumentInsertPosition::END));
  if (FILE *P = popen("xcrun --show-sdk-path 2>/dev/null", "r")) {
    char Buf[256] = {};
    if (fgets(Buf, sizeof(Buf), P) && Buf[0] == '/') {
      std::string SDK(Buf);
      SDK.erase(SDK.find_last_not_of('\n') + 1);
      Tool.appendArgumentsAdjuster(getInsertArgumentAdjuster(
          {"-isysroot", SDK}, ArgumentInsertPosition::END));
    }
    pclose(P);
  }
}

class Callback : public MatchFinder::MatchCallback {
public:
  void run(const MatchFinder::MatchResult &Result) override {
    // Retrieve whatever the matcher bound, e.g.:
    if (const auto *FD = Result.Nodes.getNodeAs<FunctionDecl>("fn")) {
      llvm::outs() << "matched function: " << FD->getQualifiedNameAsString()
                   << "\n";
    }
  }
};

int main(int argc, const char **argv) {
  auto Options = CommonOptionsParser::create(argc, argv, ToolCategory);
  if (!Options) {
    llvm::errs() << llvm::toString(Options.takeError());
    return 1;
  }

  MatchFinder Finder;
  Callback CB;
  // Placeholder matcher — replace with your own.
  Finder.addMatcher(
      functionDecl(isExpansionInMainFile()).bind("fn"), &CB);

  ClangTool Tool(Options->getCompilations(), Options->getSourcePathList());
  addPlatformFlags(Tool);

  return Tool.run(newFrontendActionFactory(&Finder).get());
}
