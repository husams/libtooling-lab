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
#include "clang/Index/USRGeneration.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/CommandLine.h"

#include <dlfcn.h>
#include <sqlite3.h>
#include <string>
#include <vector>

using namespace clang;
using namespace clang::tooling;

static llvm::cl::OptionCategory ToolCategory("visitor-tool options");

// One row of the symbol table. The USR is the stable, cross-TU identity — the
// key we'll use in SQLite so re-running over more files merges cleanly.
struct Symbol {
  std::string USR;
  std::string Name;   // fully-qualified
  std::string Kind;   // decl kind name, e.g. "Function", "CXXRecord"
  std::string File;
  unsigned Line = 0;
  unsigned Col = 0;
  bool IsDefinition = false;
};

// One use-site: a DeclRefExpr / MemberExpr pointing back at a declaration's
// USR. This is the other half of a cross-reference index — decl -> uses.
struct Reference {
  std::string USR;    // USR of the referenced declaration
  std::string Kind;   // "DeclRef" or "Member"
  std::string File;
  unsigned Line = 0;
  unsigned Col = 0;
};

class Visitor : public RecursiveASTVisitor<Visitor> {
public:
  Visitor(ASTContext &Ctx, std::vector<Symbol> &Decls,
          std::vector<Reference> &Refs)
      : Ctx(Ctx), Decls(Decls), Refs(Refs) {}

  // Fire on every named declaration; RecursiveASTVisitor dispatches every
  // FunctionDecl/VarDecl/RecordDecl/... through here via the NamedDecl base.
  bool VisitNamedDecl(NamedDecl *ND) {
    SourceManager &SM = Ctx.getSourceManager();
    SourceLocation Loc = ND->getLocation();
    if (!SM.isInMainFile(Loc))
      return true;

    // USR without the Index *tool* — just the generator. Returns true on
    // failure (e.g. decls that have no stable USR); skip those.
    llvm::SmallString<128> USR;
    if (index::generateUSRForDecl(ND, USR))
      return true;

    PresumedLoc PL = SM.getPresumedLoc(Loc);
    Symbol S;
    S.USR = std::string(USR);
    S.Name = ND->getQualifiedNameAsString();
    S.Kind = ND->getDeclKindName();
    S.File = PL.isValid() ? PL.getFilename() : "";
    S.Line = PL.isValid() ? PL.getLine() : 0;
    S.Col = PL.isValid() ? PL.getColumn() : 0;
    // "Is this the definition?" is asked differently per kind — there's no
    // single Decl-level predicate.
    if (const auto *FD = dyn_cast<FunctionDecl>(ND))
      S.IsDefinition = FD->isThisDeclarationADefinition();
    else if (const auto *TD = dyn_cast<TagDecl>(ND))
      S.IsDefinition = TD->isThisDeclarationADefinition();
    else if (const auto *VD = dyn_cast<VarDecl>(ND))
      S.IsDefinition = VD->isThisDeclarationADefinition();
    Decls.push_back(std::move(S));
    return true;
  }

  // A name used as a value: variable read, function called, enum constant.
  bool VisitDeclRefExpr(DeclRefExpr *DRE) {
    recordUse(DRE->getDecl(), DRE->getLocation(), "DeclRef");
    return true;
  }

  // A member accessed with . or ->: field or method.
  bool VisitMemberExpr(MemberExpr *ME) {
    recordUse(ME->getMemberDecl(), ME->getMemberLoc(), "Member");
    return true;
  }

private:
  void recordUse(const NamedDecl *Target, SourceLocation Loc,
                 const char *Kind) {
    SourceManager &SM = Ctx.getSourceManager();
    if (!Target || !SM.isInMainFile(Loc))
      return;
    llvm::SmallString<128> USR;
    if (index::generateUSRForDecl(Target, USR))
      return;
    PresumedLoc PL = SM.getPresumedLoc(Loc);
    Reference R;
    R.USR = std::string(USR);
    R.Kind = Kind;
    R.File = PL.isValid() ? PL.getFilename() : "";
    R.Line = PL.isValid() ? PL.getLine() : 0;
    R.Col = PL.isValid() ? PL.getColumn() : 0;
    Refs.push_back(std::move(R));
  }

  ASTContext &Ctx;
  std::vector<Symbol> &Decls;
  std::vector<Reference> &Refs;
};

// Persist the collected symbols into SQLite. INSERT OR REPLACE keyed on the
// USR means a definition seen in a later TU overwrites an earlier declaration.
static void writeToSqlite(const std::vector<Symbol> &Symbols,
                          const std::vector<Reference> &Refs,
                          const std::string &DbPath) {
  sqlite3 *DB = nullptr;
  if (sqlite3_open(DbPath.c_str(), &DB) != SQLITE_OK) {
    llvm::errs() << "sqlite open failed: " << sqlite3_errmsg(DB) << "\n";
    sqlite3_close(DB);
    return;
  }
  sqlite3_exec(DB,
               "CREATE TABLE IF NOT EXISTS symbols("
               "usr TEXT PRIMARY KEY, name TEXT, kind TEXT, "
               "file TEXT, line INTEGER, col INTEGER, is_def INTEGER);"
               // A use-site has no natural key; UNIQUE on usr+location lets us
               // re-run idempotently with INSERT OR IGNORE.
               "CREATE TABLE IF NOT EXISTS refs("
               "usr TEXT, kind TEXT, file TEXT, line INTEGER, col INTEGER, "
               "UNIQUE(usr, file, line, col));",
               nullptr, nullptr, nullptr);

  sqlite3_exec(DB, "BEGIN;", nullptr, nullptr, nullptr);
  sqlite3_stmt *Ins = nullptr;
  sqlite3_prepare_v2(DB,
                     "INSERT OR REPLACE INTO symbols VALUES(?,?,?,?,?,?,?);",
                     -1, &Ins, nullptr);
  for (const Symbol &S : Symbols) {
    sqlite3_bind_text(Ins, 1, S.USR.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(Ins, 2, S.Name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(Ins, 3, S.Kind.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(Ins, 4, S.File.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(Ins, 5, S.Line);
    sqlite3_bind_int(Ins, 6, S.Col);
    sqlite3_bind_int(Ins, 7, S.IsDefinition ? 1 : 0);
    sqlite3_step(Ins);
    sqlite3_reset(Ins);
  }
  sqlite3_finalize(Ins);

  sqlite3_stmt *InsRef = nullptr;
  sqlite3_prepare_v2(DB,
                     "INSERT OR IGNORE INTO refs VALUES(?,?,?,?,?);", -1,
                     &InsRef, nullptr);
  for (const Reference &R : Refs) {
    sqlite3_bind_text(InsRef, 1, R.USR.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(InsRef, 2, R.Kind.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(InsRef, 3, R.File.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(InsRef, 4, R.Line);
    sqlite3_bind_int(InsRef, 5, R.Col);
    sqlite3_step(InsRef);
    sqlite3_reset(InsRef);
  }
  sqlite3_finalize(InsRef);

  sqlite3_exec(DB, "COMMIT;", nullptr, nullptr, nullptr);
  sqlite3_close(DB);
  llvm::outs() << "wrote " << Symbols.size() << " symbols, " << Refs.size()
               << " refs to " << DbPath << "\n";
}

class Consumer : public ASTConsumer {
public:
  void HandleTranslationUnit(ASTContext &Ctx) override {
    std::vector<Symbol> Symbols;
    std::vector<Reference> Refs;
    Visitor V(Ctx, Symbols, Refs);
    V.TraverseDecl(Ctx.getTranslationUnitDecl());
    writeToSqlite(Symbols, Refs, "symbols.db");
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
