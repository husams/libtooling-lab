// visitor-tool — skeleton for a RecursiveASTVisitor-based Clang tool.
//
// The wiring is complete and compiles as-is; it just doesn't visit anything
// yet. Add VisitXxx methods to Visitor and rebuild.
//
// Run:
//   ./build/visitor-tool sample/sample.cpp -- -std=c++17

#include "clang/AST/ASTConsumer.h"
#include "clang/Options/OptionUtils.h"
#include "clang/Tooling/ArgumentsAdjusters.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"

#include <dlfcn.h>

using namespace clang;
using namespace clang::tooling;

static llvm::cl::OptionCategory ToolCategory("visitor-tool options");

class Visitor : public RecursiveASTVisitor<Visitor> {
public:
  explicit Visitor(ASTContext &Ctx) : Ctx(Ctx) {}

  // Add visit methods here. Return true to keep traversing. For example:
  //
  bool VisitFunctionDecl(FunctionDecl *FD) {
    SourceManager &sm = Ctx.getSourceManager();
    SourceLocation loc = FD->getLocation();
  
    if (!sm.isInMainFile(loc)){
      return true;
    }
    llvm::outs() << FD->getQualifiedNameAsString() << "\n";
    return true;
  }

private:
  ASTContext &Ctx;
};

class Consumer : public ASTConsumer {
public:
  void HandleTranslationUnit(ASTContext &Ctx) override {
    Visitor V(Ctx);
    V.TraverseDecl(Ctx.getTranslationUnitDecl());
  }
};

class Action : public ASTFrontendAction {
public:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 StringRef InFile) override {
    return std::make_unique<Consumer>();
  }
};

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

int main(int argc, const char **argv) {
  auto Options = CommonOptionsParser::create(argc, argv, ToolCategory);
  if (!Options) {
    llvm::errs() << llvm::toString(Options.takeError());
    return 1;
  }
  ClangTool Tool(Options->getCompilations(), Options->getSourcePathList());
  addPlatformFlags(Tool);

  return Tool.run(newFrontendActionFactory<Action>().get());
}
