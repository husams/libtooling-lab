// cpp-modernize.cpp — the capstone: a small clang-tidy-style linter (Part 6).
// Three checks over a real multi-file project (via compile_commands.json):
//   [null]      NULL  -> nullptr            (auto-fixable)
//   [override]  add missing `override`      (auto-fixable)
//   [params]    too many function params    (report only)
//
// Run read-only for a report, or with --fix to rewrite the sources on disk.
// Because headers are parsed once per translation unit, the same finding can
// surface multiple times; we de-duplicate by file+offset+check so each issue
// is reported and fixed exactly once.
#include "clang/AST/Attr.h"
#include "clang/AST/Expr.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Refactoring.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include <set>
#include <string>

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;
using namespace llvm;

static cl::OptionCategory Cat("cpp-modernize options");
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);
static cl::opt<bool> Fix("fix", cl::desc("Apply fixes to disk"), cl::cat(Cat));
static cl::opt<unsigned> MaxParams(
    "max-params", cl::desc("Max allowed function parameters (default 4)"),
    cl::init(4), cl::cat(Cat));

struct Stats { unsigned null = 0, override_ = 0, params = 0; };

class Check : public MatchFinder::MatchCallback {
public:
    Check(std::map<std::string, Replacements> &R, Stats &S)
        : Repls_(R), Stats_(S) {}

    void run(const MatchFinder::MatchResult &Res) override {
        SM_ = Res.SourceManager;
        DE_ = &Res.Context->getDiagnostics();
        if (const auto *NE = Res.Nodes.getNodeAs<GNUNullExpr>("null"))
            checkNull(NE);
        if (const auto *M = Res.Nodes.getNodeAs<CXXMethodDecl>("ovr"))
            checkOverride(M);
        if (const auto *FD = Res.Nodes.getNodeAs<FunctionDecl>("fn"))
            checkParams(FD);
    }

private:
    // True the first time we see this (file, offset, check) triple.
    bool firstTime(SourceLocation L, StringRef check) {
        std::string key = std::string(SM_->getFilename(L)) + ":" +
                          std::to_string(SM_->getFileOffset(L)) + ":" +
                          std::string(check);
        return Seen_.insert(key).second;
    }

    bool ours(SourceLocation L) const {
        // Lint our own code, never system headers (<string>, <cstddef>, ...).
        return L.isValid() && !SM_->isInSystemHeader(L);
    }

    void warn(SourceLocation L, const std::string &Msg) {
        unsigned ID = DE_->getCustomDiagID(DiagnosticsEngine::Warning, "%0");
        DE_->Report(L, ID) << Msg;
    }
    void warnFix(SourceLocation L, const std::string &Msg, CharSourceRange R,
                 StringRef Repl) {
        unsigned ID = DE_->getCustomDiagID(DiagnosticsEngine::Warning, "%0");
        DE_->Report(L, ID) << Msg << FixItHint::CreateReplacement(R, Repl);
    }
    void record(SourceLocation L, CharSourceRange R, StringRef Text) {
        auto Err = Repls_[std::string(SM_->getFilename(L))].add(
            Replacement(*SM_, R, Text));
        if (Err) consumeError(std::move(Err));
    }

    void checkNull(const GNUNullExpr *NE) {
        // NULL is a macro -> __null; use the EXPANSION range to land on the
        // `NULL` token as written, not the header where NULL is #defined.
        CharSourceRange R = SM_->getExpansionRange(NE->getSourceRange());
        SourceLocation B = R.getBegin();
        if (!ours(B) || !firstTime(B, "null")) return;
        warnFix(B, "[null] use nullptr instead of NULL", R, "nullptr");
        record(B, R, "nullptr");
        ++Stats_.null;
    }

    void checkOverride(const CXXMethodDecl *M) {
        if (M->hasAttr<OverrideAttr>() || M->hasAttr<FinalAttr>()) return;
        if (!M->hasBody()) return;  // keep the insertion point simple
        SourceLocation L = M->getBeginLoc();
        if (!ours(L) || !firstTime(L, "override")) return;
        // Insert "override " right before the body's opening brace, which sits
        // after the parameter list and any trailing const/noexcept.
        SourceLocation Brace = M->getBody()->getBeginLoc();
        CharSourceRange R = CharSourceRange::getCharRange(Brace, Brace);
        warnFix(L, "[override] missing 'override' specifier", R, "override ");
        record(Brace, R, "override ");
        ++Stats_.override_;
    }

    void checkParams(const FunctionDecl *FD) {
        if (!FD->isThisDeclarationADefinition()) return;
        SourceLocation L = FD->getBeginLoc();
        if (!ours(L) || !firstTime(L, "params")) return;
        if (FD->getNumParams() <= MaxParams) return;
        warn(L, "[params] " + FD->getNameAsString() + " has " +
                    std::to_string(FD->getNumParams()) + " parameters (max " +
                    std::to_string(MaxParams.getValue()) + ")");
        ++Stats_.params;
    }

    SourceManager *SM_ = nullptr;
    DiagnosticsEngine *DE_ = nullptr;
    std::map<std::string, Replacements> &Repls_;
    Stats &Stats_;
    std::set<std::string> Seen_;
};

int main(int argc, const char **argv) {
    auto ExpectedParser = CommonOptionsParser::create(argc, argv, Cat);
    if (!ExpectedParser) { errs() << ExpectedParser.takeError(); return 1; }
    CommonOptionsParser &OP = ExpectedParser.get();

    RefactoringTool Tool(OP.getCompilations(), OP.getSourcePathList());
    Stats S;
    Check C(Tool.getReplacements(), S);

    MatchFinder Finder;
    Finder.addMatcher(gnuNullExpr().bind("null"), &C);
    Finder.addMatcher(
        cxxMethodDecl(isOverride(), unless(cxxDestructorDecl())).bind("ovr"),
        &C);
    Finder.addMatcher(functionDecl(isDefinition()).bind("fn"), &C);

    int rc = Fix ? Tool.runAndSave(newFrontendActionFactory(&Finder).get())
                 : Tool.run(newFrontendActionFactory(&Finder).get());

    errs() << "\n=== cpp-modernize summary ===\n"
           << "  [null]     " << S.null << "\n"
           << "  [override] " << S.override_ << "\n"
           << "  [params]   " << S.params << "\n"
           << (Fix ? "fixes applied to disk\n"
                   : "run with --fix to apply auto-fixable findings\n");
    return rc;
}
