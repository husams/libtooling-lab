// fixnull.cpp — detect NULL and turn it into nullptr (Part 5).
// Demonstrates three things at once:
//   * a custom compiler-style diagnostic (DiagnosticsEngine)
//   * a fix-it hint (FixItHint) so clang/editors can show the suggested edit
//   * real on-disk edits via RefactoringTool + Replacements (with --fix)
//
// In C++ on Linux/GCC headers, the NULL macro expands to the builtin __null,
// which Clang models as a GNUNullExpr node. We match that node and rewrite the
// `NULL` token the programmer actually wrote — found via its EXPANSION range
// (the spelling location would point into the header where NULL is #defined).
#include "clang/AST/Expr.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Refactoring.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;
using namespace llvm;

static cl::OptionCategory ToolCategory("fixnull options");
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);
static cl::opt<bool> Fix("fix", cl::desc("Apply fixes to disk"),
                         cl::cat(ToolCategory));

class NullFixer : public MatchFinder::MatchCallback {
public:
    explicit NullFixer(std::map<std::string, Replacements> &Repls)
        : Repls_(Repls) {}

    void run(const MatchFinder::MatchResult &R) override {
        const auto *NE = R.Nodes.getNodeAs<GNUNullExpr>("null");
        if (!NE) return;
        SourceManager &SM = *R.SourceManager;

        // NULL is a macro expanding to __null. Its spelling location points
        // into the header where NULL is #defined, so use the EXPANSION range to
        // get the `NULL` token as written in this file.
        CharSourceRange Range = SM.getExpansionRange(NE->getSourceRange());
        SourceLocation Begin = Range.getBegin();
        if (!SM.isInMainFile(Begin)) return;

        // 1) Report it like the compiler would, with an attached fix-it.
        DiagnosticsEngine &DE = R.Context->getDiagnostics();
        unsigned ID = DE.getCustomDiagID(DiagnosticsEngine::Warning,
                                         "use nullptr instead of NULL");
        DE.Report(Begin, ID) << FixItHint::CreateReplacement(Range, "nullptr");

        // 2) Record an on-disk edit (applied only when --fix is given).
        auto Err = Repls_[std::string(SM.getFilename(Begin))].add(
            Replacement(SM, Range, "nullptr"));
        if (Err) consumeError(std::move(Err));
    }

private:
    std::map<std::string, Replacements> &Repls_;
};

int main(int argc, const char **argv) {
    auto ExpectedParser = CommonOptionsParser::create(argc, argv, ToolCategory);
    if (!ExpectedParser) { errs() << ExpectedParser.takeError(); return 1; }
    CommonOptionsParser &OP = ExpectedParser.get();

    // RefactoringTool (not plain ClangTool) owns the Replacements map and can
    // apply edits to disk with runAndSave().
    RefactoringTool Tool(OP.getCompilations(), OP.getSourcePathList());
    NullFixer Fixer(Tool.getReplacements());
    MatchFinder Finder;
    Finder.addMatcher(gnuNullExpr().bind("null"), &Fixer);

    if (Fix)
        return Tool.runAndSave(newFrontendActionFactory(&Finder).get());
    return Tool.run(newFrontendActionFactory(&Finder).get());
}
