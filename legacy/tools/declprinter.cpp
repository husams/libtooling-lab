// declprinter.cpp — walk the AST by hand with RecursiveASTVisitor (Part 2).
// Prints every function and C++ class *defined in the main file*, with its
// source location. Demonstrates the four-class LibTooling pattern:
//   ASTFrontendAction -> ASTConsumer -> RecursiveASTVisitor (+ ASTContext)
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"

using namespace clang;
using namespace clang::tooling;
using namespace llvm;

// 1) VISITOR — CRTP class; override VisitXxx() for the node kinds you care
//    about. Return true to keep traversing, false to prune the subtree.
class DeclVisitor : public RecursiveASTVisitor<DeclVisitor> {
public:
    explicit DeclVisitor(ASTContext &Ctx)
        : SM_(Ctx.getSourceManager()) {}

    bool VisitFunctionDecl(FunctionDecl *FD) {
        if (FD->isThisDeclarationADefinition())  // skip bare prototypes
            report("function", FD->getQualifiedNameAsString(), FD->getBeginLoc());
        return true;
    }

    bool VisitCXXRecordDecl(CXXRecordDecl *RD) {
        if (RD->isThisDeclarationADefinition())
            report("class", RD->getQualifiedNameAsString(), RD->getBeginLoc());
        return true;
    }

private:
    void report(StringRef kind, const std::string &name, SourceLocation loc) {
        // Main-file filter: ignore everything pulled in from headers, or you'd
        // print thousands of decls from <string>, <vector>, etc.
        if (!SM_.isInMainFile(loc)) return;
        outs() << kind << "  " << name << "  ("
               << SM_.getFilename(loc) << ":"
               << SM_.getSpellingLineNumber(loc) << ")\n";
    }

    SourceManager &SM_;
};

// 2) CONSUMER — receives the fully-parsed translation unit and drives the
//    visitor from the top-level TranslationUnitDecl.
class DeclConsumer : public ASTConsumer {
public:
    explicit DeclConsumer(ASTContext &Ctx) : Visitor_(Ctx) {}
    void HandleTranslationUnit(ASTContext &Ctx) override {
        Visitor_.TraverseDecl(Ctx.getTranslationUnitDecl());
    }
private:
    DeclVisitor Visitor_;
};

// 3) ACTION — the factory the tool runs; creates the consumer per file.
class DeclAction : public ASTFrontendAction {
public:
    std::unique_ptr<ASTConsumer>
    CreateASTConsumer(CompilerInstance &CI, StringRef /*file*/) override {
        return std::make_unique<DeclConsumer>(CI.getASTContext());
    }
};

static cl::OptionCategory ToolCategory("declprinter options");
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

int main(int argc, const char **argv) {
    auto ExpectedParser = CommonOptionsParser::create(argc, argv, ToolCategory);
    if (!ExpectedParser) { errs() << ExpectedParser.takeError(); return 1; }
    CommonOptionsParser &OP = ExpectedParser.get();
    ClangTool Tool(OP.getCompilations(), OP.getSourcePathList());
    return Tool.run(newFrontendActionFactory<DeclAction>().get());
}
