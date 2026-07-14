// injectlog.cpp — source-to-source rewriting with Rewriter (Part 4).
// Inserts a comment-marker at the top of every function body defined in the
// main file, then prints the rewritten source to stdout. Shows the Rewriter
// lifecycle and the getRewriteBufferFor() null-check.
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"

using namespace clang;
using namespace clang::tooling;
using namespace llvm;

class LogVisitor : public RecursiveASTVisitor<LogVisitor> {
public:
    explicit LogVisitor(Rewriter &RW) : RW_(RW), SM_(RW.getSourceMgr()) {}

    bool VisitFunctionDecl(FunctionDecl *FD) {
        if (!FD->hasBody()) return true;
        if (!SM_.isInMainFile(FD->getBeginLoc())) return true;

        // The body is a CompoundStmt; its begin loc is the opening '{'.
        // Offsetting by 1 lands us just inside the brace.
        Stmt *Body = FD->getBody();
        SourceLocation AfterBrace = Body->getBeginLoc().getLocWithOffset(1);
        std::string Note = "  /* entering " + FD->getNameAsString() + " */";
        RW_.InsertText(AfterBrace, Note, /*InsertAfter=*/true);
        return true;
    }

private:
    Rewriter &RW_;
    SourceManager &SM_;
};

class LogConsumer : public ASTConsumer {
public:
    explicit LogConsumer(Rewriter &RW) : Visitor_(RW) {}
    void HandleTranslationUnit(ASTContext &Ctx) override {
        Visitor_.TraverseDecl(Ctx.getTranslationUnitDecl());
    }
private:
    LogVisitor Visitor_;
};

class LogAction : public ASTFrontendAction {
public:
    std::unique_ptr<ASTConsumer>
    CreateASTConsumer(CompilerInstance &CI, StringRef /*file*/) override {
        // Wire the Rewriter to this file's SourceManager before consuming.
        RW_.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
        return std::make_unique<LogConsumer>(RW_);
    }

    void EndSourceFileAction() override {
        SourceManager &SM = RW_.getSourceMgr();
        // getRewriteBufferFor returns nullptr if the file was never touched.
        if (const RewriteBuffer *Buf =
                RW_.getRewriteBufferFor(SM.getMainFileID()))
            Buf->write(outs());
        // To edit files in place instead, call: RW_.overwriteChangedFiles();
    }

private:
    Rewriter RW_;
};

static cl::OptionCategory ToolCategory("injectlog options");
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

int main(int argc, const char **argv) {
    auto ExpectedParser = CommonOptionsParser::create(argc, argv, ToolCategory);
    if (!ExpectedParser) { errs() << ExpectedParser.takeError(); return 1; }
    CommonOptionsParser &OP = ExpectedParser.get();
    ClangTool Tool(OP.getCompilations(), OP.getSourcePathList());
    return Tool.run(newFrontendActionFactory<LogAction>().get());
}
