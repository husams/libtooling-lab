// matchfns.cpp — the same job as declprinter, but with AST Matchers (Part 3).
// Far less code: declare matchers, bind the nodes you want, then handle them
// in a MatchCallback. No hand-written traversal.
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;
using namespace llvm;

// The callback fires once per match. R.Nodes holds the bound nodes by name.
class Printer : public MatchFinder::MatchCallback {
public:
    void run(const MatchFinder::MatchResult &R) override {
        SourceManager &SM = *R.SourceManager;
        if (const auto *FD = R.Nodes.getNodeAs<FunctionDecl>("fn"))
            report(SM, "function", FD->getQualifiedNameAsString(), FD->getBeginLoc());
        if (const auto *RD = R.Nodes.getNodeAs<CXXRecordDecl>("cls"))
            report(SM, "class", RD->getQualifiedNameAsString(), RD->getBeginLoc());
    }

private:
    void report(SourceManager &SM, StringRef kind, const std::string &name,
                SourceLocation loc) {
        if (!SM.isInMainFile(loc)) return;
        outs() << kind << "  " << name << "  ("
               << SM.getFilename(loc) << ":"
               << SM.getSpellingLineNumber(loc) << ")\n";
    }
};

static cl::OptionCategory ToolCategory("matchfns options");
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

int main(int argc, const char **argv) {
    auto ExpectedParser = CommonOptionsParser::create(argc, argv, ToolCategory);
    if (!ExpectedParser) { errs() << ExpectedParser.takeError(); return 1; }
    CommonOptionsParser &OP = ExpectedParser.get();
    ClangTool Tool(OP.getCompilations(), OP.getSourcePathList());

    Printer P;
    MatchFinder Finder;
    // Bind every function/class definition and hand it to the printer. These
    // matcher expressions are exactly what you'd prototype in clang-query.
    // unless(isImplicit()) drops compiler-generated members (implicit copy/move
    // constructors, operator=, ...). Unlike RecursiveASTVisitor — which skips
    // implicit code by default — matchers see it, so we filter it here to get
    // the same result as Part 2's declprinter.
    Finder.addMatcher(
        functionDecl(isDefinition(), unless(isImplicit())).bind("fn"), &P);
    Finder.addMatcher(
        cxxRecordDecl(isDefinition(), unless(isImplicit())).bind("cls"), &P);

    // newFrontendActionFactory(&Finder) — the matcher-mode overload.
    return Tool.run(newFrontendActionFactory(&Finder).get());
}
