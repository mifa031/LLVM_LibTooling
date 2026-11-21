#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/StmtVisitor.h"
#include <map>
#include <vector>

using namespace std;
using namespace clang;
using namespace clang::tooling;
using namespace llvm;

static cl::OptionCategory MyToolCategory("analyze options");

class BodyVisitor : public ConstStmtVisitor<BodyVisitor> {
public:
    unsigned statements = 0;
    unsigned loops = 0;

    void count(const Stmt *stmt) {
        if (!stmt)
            return;
        stmt = stmt->stripLabelLikeStatements();
        if (!isa<CompoundStmt>(stmt))
            ++statements;
        Visit(stmt);
    }

    void VisitCompoundStmt(const CompoundStmt *block) {
        for (const Stmt *child : block->body())
            count(child);
    }

    void VisitIfStmt(const IfStmt *branch) {
        count(branch->getThen());
        count(branch->getElse());
    }

    void VisitSwitchStmt(const SwitchStmt *stmt) { count(stmt->getBody()); }

    void VisitCXXTryStmt(const CXXTryStmt *stmt) {
        count(stmt->getTryBlock());
        for (unsigned i = 0; i < stmt->getNumHandlers(); ++i)
            count(stmt->getHandler(i)->getHandlerBlock());
    }

    void VisitForStmt(const ForStmt *loop) { countLoop(loop->getBody()); }
    void VisitWhileStmt(const WhileStmt *loop) { countLoop(loop->getBody()); }
    void VisitDoStmt(const DoStmt *loop) { countLoop(loop->getBody()); }
    void VisitCXXForRangeStmt(const CXXForRangeStmt *loop) { countLoop(loop->getBody()); }

private:
    void countLoop(const Stmt *body) {
        ++loops;
        count(body);
    }
};

class AnalysisConsumer : public ASTConsumer, public RecursiveASTVisitor<AnalysisConsumer> {
    const SourceManager &source;
    std::vector<FunctionDecl *> functions;
    std::map<const FunctionDecl *, unsigned> calls;

public:
    explicit AnalysisConsumer(const SourceManager &source) : source(source) {}

    bool VisitFunctionDecl(FunctionDecl *function) {
        if (source.isInMainFile(function->getLocation()) && !function->isImplicit() &&
            function->doesThisDeclarationHaveABody())
            functions.push_back(function);
        return true;
    }

    bool VisitCallExpr(CallExpr *call) {
        const FunctionDecl *callee = call->getDirectCallee();
        if (callee && source.isInMainFile(call->getExprLoc()))
            ++calls[callee->getCanonicalDecl()];
        return true;
    }
    void HandleTranslationUnit(ASTContext &context) override {
        if (context.getDiagnostics().hasErrorOccurred())
            return;

        TraverseDecl(context.getTranslationUnitDecl());
        llvm::outs() << "function\targuments\tstatements\tloops\tcalls\n";
        for (FunctionDecl *function : functions) {
            BodyVisitor body;
            body.count(function->getBody());
            llvm::outs() << function->getQualifiedNameAsString() << '\t'
                         << function->getNumParams() << '\t' << body.statements << '\t'
                         << body.loops << '\t' << calls[function->getCanonicalDecl()] << '\n';
        }
    }
};

class AnalysisAction : public ASTFrontendAction {
    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &compiler, llvm::StringRef) override {
        return std::make_unique<AnalysisConsumer>(compiler.getSourceManager());
    }
};

int main(int argc, const char **argv) {

  // parse command-line args passed to your code
  auto parsed = CommonOptionsParser::create(argc, argv, MyToolCategory);
  if (!parsed) {
    errs() << toString(parsed.takeError()) << '\n';
    return 1;
  }
  CommonOptionsParser &op = parsed.get();

  // create a new LibTooling instance
  ClangTool Tool(op.getCompilations(), op.getSourcePathList());

  // run the LibTool
  // the tool creates a new FrontEndAction
  // need the return value to send to LLVM
  int result = Tool.run(newFrontendActionFactory<AnalysisAction>().get());

  // send result to LLVM
  return result;
}
