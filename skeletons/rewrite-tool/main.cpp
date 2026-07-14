// rewrite-tool — skeleton for a clang/Rewrite-based source-rewriting tool.
//
// The wiring is complete and compiles as-is: it inserts a marker comment
// before every function *definition* in the main file, then prints the
// rewritten file to stdout. Nothing on disk changes — swap in
// overwriteChangedFiles() (Part 38) once your edits are real. Grow it by
// editing Visitor.
//
// Run:
//   ./build/rewrite-tool sample/sample.cpp -- -std=c++17

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Options/OptionUtils.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/ArgumentsAdjusters.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/ADT/RewriteBuffer.h"
#include "llvm/Support/CommandLine.h"

#include <dlfcn.h>

using namespace clang;
using namespace clang::tooling;

static llvm::cl::OptionCategory ToolCategory("rewrite-tool options");

// The Rewriter records edits as (SourceLocation, text) operations against the
// SourceManager's buffers; it never mutates the AST. InsertText / ReplaceText
// / RemoveText are all you need — the buffer replays them on demand.
class Visitor : public RecursiveASTVisitor<Visitor> {
public:
  Visitor(ASTContext &Ctx, Rewriter &R) : Ctx(Ctx), R(R) {}

  bool VisitFunctionDecl(FunctionDecl *FD) {
    SourceManager &SM = Ctx.getSourceManager();
    if (!FD->isThisDeclarationADefinition())
      return true;
    SourceLocation Loc = FD->getBeginLoc();
    if (!SM.isWrittenInMainFile(Loc))
      return true;
    R.InsertText(Loc, "/* [rewritten] */ ", /*InsertAfter=*/true);
    return true;
  }

private:
  ASTContext &Ctx;
  Rewriter &R;
};

class Consumer : public ASTConsumer {
public:
  void HandleTranslationUnit(ASTContext &Ctx) override {
    Rewriter R(Ctx.getSourceManager(), Ctx.getLangOpts());
    Visitor V(Ctx, R);
    V.TraverseDecl(Ctx.getTranslationUnitDecl());

    // Replay the recorded edits over the main file and print the result.
    FileID FID = Ctx.getSourceManager().getMainFileID();
    if (const llvm::RewriteBuffer *Buf = R.getRewriteBufferFor(FID))
      Buf->write(llvm::outs());
    else
      llvm::outs() << "(no edits recorded)\n";
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
