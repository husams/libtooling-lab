// firsttool.cpp — the smallest useful LibTooling program (Part 1).
// It builds a ClangTool from the command line, then runs Clang's built-in
// SyntaxOnlyAction over each input file. No output means "parsed cleanly";
// a syntax error in the input surfaces as an ordinary compiler diagnostic.
#include "clang/Frontend/FrontendActions.h"      // clang::SyntaxOnlyAction
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"

using namespace clang::tooling;
using namespace llvm;

// Every LibTooling tool declares an option category so --help is scoped to the
// tool's own flags instead of all of LLVM's internal options.
static cl::OptionCategory ToolCategory("firsttool options");
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

int main(int argc, const char **argv) {
    // create() (LLVM 10+) returns llvm::Expected<> — you MUST check it before
    // calling .get(), or the program aborts.
    auto ExpectedParser = CommonOptionsParser::create(argc, argv, ToolCategory);
    if (!ExpectedParser) {
        errs() << ExpectedParser.takeError();
        return 1;
    }
    CommonOptionsParser &OptionsParser = ExpectedParser.get();

    // A ClangTool is built from a compilation database + a list of source files.
    ClangTool Tool(OptionsParser.getCompilations(),
                   OptionsParser.getSourcePathList());

    // Run a front-end action over every source file. SyntaxOnlyAction parses
    // and type-checks but emits no code. run() returns 0 on success.
    return Tool.run(
        newFrontendActionFactory<clang::SyntaxOnlyAction>().get());
}
