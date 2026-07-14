// cfg-tool — skeleton for a clang/Analysis-based tool (CFG + CallGraph).
//
// The wiring is complete and compiles as-is: it builds a control-flow graph
// for every function definition in the main file and prints each graph's
// blocks and their successor edges, then prints the translation unit's call
// graph. Grow it by editing Visitor / the CallGraph section.
//
// Run:
//   ./build/cfg-tool sample/sample.cpp -- -std=c++17

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Analysis/CallGraph.h"
#include "clang/Analysis/CFG.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Options/OptionUtils.h"
#include "clang/Tooling/ArgumentsAdjusters.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"

#include <dlfcn.h>

using namespace clang;
using namespace clang::tooling;

static llvm::cl::OptionCategory ToolCategory("cfg-tool options");

// A CFG is built per function body, on demand — it is not part of the AST.
// buildCFG lowers the body's statements into basic blocks joined by edges;
// every branch, loop, and short-circuit becomes explicit control flow.
class Visitor : public RecursiveASTVisitor<Visitor> {
public:
  explicit Visitor(ASTContext &Ctx) : Ctx(Ctx) {}

  bool VisitFunctionDecl(FunctionDecl *FD) {
    if (!FD->doesThisDeclarationHaveABody())
      return true;
    if (!Ctx.getSourceManager().isInMainFile(FD->getLocation()))
      return true;

    std::unique_ptr<CFG> Graph =
        CFG::buildCFG(FD, FD->getBody(), &Ctx, CFG::BuildOptions());
    if (!Graph)
      return true;

    llvm::outs() << FD->getQualifiedNameAsString() << ": " << Graph->size()
                 << " blocks\n";
    for (const CFGBlock *B : *Graph) {
      llvm::outs() << "  B" << B->getBlockID() << " -> ";
      for (const CFGBlock *Succ : B->succs())
        if (Succ)                          // an edge can be unreachable (null)
          llvm::outs() << "B" << Succ->getBlockID() << " ";
      llvm::outs() << "\n";
    }
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

    // The whole-TU view: who calls whom. CallGraph walks the AST itself and
    // indexes *every* function it can reach — including the std internals
    // pulled in by the headers — so filter callers down to this file.
    llvm::outs() << "\n--- call graph (main file) ---\n";
    SourceManager &SM = Ctx.getSourceManager();
    CallGraph CG;
    CG.addToCallGraph(Ctx.getTranslationUnitDecl());
    for (const CallGraphNode *N : *CG.getRoot()) {
      const auto *FD = llvm::dyn_cast_or_null<FunctionDecl>(N->getDecl());
      if (!FD || !SM.isInMainFile(FD->getLocation()))
        continue;
      llvm::outs() << FD->getQualifiedNameAsString() << " calls " << N->size()
                   << " function(s)\n";
    }
  }
};

class Action : public ASTFrontendAction {
public:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &,
                                                 StringRef) override {
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
