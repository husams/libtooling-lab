// index-tool — skeleton for a clang/Index-based Clang tool.
//
// The wiring is complete and compiles as-is: it prints every symbol
// *occurrence* (declaration, reference, call, ...) in the main file, with the
// symbol's kind, the roles it plays at that spot, and its USR — the stable
// cross-TU identity string. Grow it by editing IndexConsumer.
//
// Run:
//   ./build/index-tool sample/sample.cpp -- -std=c++17

#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Index/IndexDataConsumer.h"
#include "clang/Index/IndexingAction.h"
#include "clang/Index/IndexingOptions.h"
#include "clang/Index/IndexSymbol.h"
#include "clang/Index/USRGeneration.h"
#include "clang/Options/OptionUtils.h"
#include "clang/Tooling/ArgumentsAdjusters.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"

#include <dlfcn.h>

using namespace clang;
using namespace clang::index;
using namespace clang::tooling;

static llvm::cl::OptionCategory ToolCategory("index-tool options");

// One callback per *use* of a symbol. The Index library resolves each spelled
// name to its canonical Decl for you, tags it with the roles it plays here
// (Declaration / Definition / Reference / Call / ...), and hands you the
// source location. Everything below is standard Decl/SourceManager territory.
class IndexConsumer : public IndexDataConsumer {
public:
  bool handleDeclOccurrence(const Decl *D, SymbolRoleSet Roles,
                            ArrayRef<SymbolRelation> /*Relations*/,
                            SourceLocation Loc, ASTNodeInfo) override {
    const SourceManager &SM = D->getASTContext().getSourceManager();
    if (!SM.isInMainFile(SM.getFileLoc(Loc)))   // occurrences in this TU only
      return true;

    SymbolInfo Info = getSymbolInfo(D);
    llvm::SmallString<128> USR;
    generateUSRForDecl(D, USR);                 // the stable identity

    llvm::outs() << SM.getFileLoc(Loc).printToString(SM) << "  "
                 << getSymbolKindString(Info.Kind) << "  [";
    printSymbolRoles(Roles, llvm::outs());
    llvm::outs() << "]  " << USR << "\n";
    return true;   // return false to abort the whole index run
  }
};

// createIndexingAction() builds a FrontendAction, but ClangTool::run() wants a
// factory that can mint a fresh action per TU. This adapter is the bridge.
class IndexActionFactory : public FrontendActionFactory {
public:
  explicit IndexActionFactory(std::shared_ptr<IndexConsumer> C)
      : Consumer(std::move(C)) {}

  std::unique_ptr<FrontendAction> create() override {
    IndexingOptions Opts;                       // defaults: main-TU decls only
    return createIndexingAction(Consumer, Opts);
  }

private:
  std::shared_ptr<IndexConsumer> Consumer;
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

  auto Consumer = std::make_shared<IndexConsumer>();
  IndexActionFactory Factory(Consumer);
  return Tool.run(&Factory);
}
