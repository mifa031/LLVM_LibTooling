#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include <string>

using namespace std;
using namespace clang;
using namespace clang::tooling;
using namespace llvm;

static cl::OptionCategory MyToolCategory("strength-reduce options");

int shiftFor(const Expr *expr) {
    const auto *number = dyn_cast<IntegerLiteral>(expr->IgnoreParenImpCasts());
    if (!number || number->getLocation().isMacroID() || !number->getValue().isPowerOf2())
        return -1;
    return number->getValue().logBase2();
}

class ShiftConsumer : public ASTConsumer, public RecursiveASTVisitor<ShiftConsumer> {
    ASTContext &context;
    Rewriter output;

public:
    explicit ShiftConsumer(ASTContext &context)
        : context(context), output(context.getSourceManager(), context.getLangOpts()) {}

    void HandleTranslationUnit(ASTContext &ast) override {
        if (!ast.getDiagnostics().hasErrorOccurred()) {
            TraverseDecl(ast.getTranslationUnitDecl());
            output.getEditBuffer(ast.getSourceManager().getMainFileID()).write(outs());
        }
    }

    bool shouldTraversePostOrder() const { return true; }

    bool VisitBinaryOperator(BinaryOperator *op) {
        bool multiply = op->getOpcode() == BO_Mul;
        if (!multiply && op->getOpcode() != BO_Div)
            return true;

        auto type = op->getType().getCanonicalType();
        if (!type->isBuiltinType() || !type->isIntegerType())
            return true;
        if (!context.getSourceManager().isWrittenInMainFile(op->getOperatorLoc()) ||
            op->getBeginLoc().isMacroID() || op->getEndLoc().isMacroID())
            return true;

        const Expr *operand = op->getLHS();
        int shift = shiftFor(op->getRHS());
        if (shift < 0 && multiply) {
            shift = shiftFor(operand);
            operand = op->getRHS();
        }
        if (shift < 0)
            return true;

        const Expr *written = operand->IgnoreUnlessSpelledInSource();
        if (!written->getType()->isIntegerType())
            return true;
        if (output.getRewrittenText(op->getSourceRange()).find('#') != string::npos)
            return true;

        bool signedMultiply = multiply && type->isSignedIntegerType();
        QualType shiftType = signedMultiply ? context.getCorrespondingUnsignedType(type) : type;
        std::string text = output.getRewrittenText(operand->getSourceRange());
        std::string value = "((" + shiftType.getAsString() + ")(" + text + "))";

        if (!multiply && type->isSignedIntegerType() && shift > 0) {
            if (!isa<DeclRefExpr>(written->IgnoreParens()) || operand->HasSideEffects(context))
                return true;
            string divisor = output.getRewrittenText(op->getRHS()->getSourceRange());
            value = "(" + value + " + (" + value + " < 0 ? (" + divisor + " - 1) : 0))";
        }
        std::string replacement = "(" + value + (multiply ? " << " : " >> ") + std::to_string(shift) + ")";
        if (signedMultiply)
            replacement = "((" + type.getAsString() + ")" + replacement + ")";
        output.ReplaceText(op->getSourceRange(), replacement);
        return true;
    }
};

class ShiftAction : public ASTFrontendAction {
    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &compiler, llvm::StringRef) override {
        return std::make_unique<ShiftConsumer>(compiler.getASTContext());
    }
};

int main(int argc, const char **argv) {

  // parse command-line args passed to your code
  auto parsed = CommonOptionsParser::create(argc, argv, MyToolCategory, cl::Required);
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
  int result = Tool.run(newFrontendActionFactory<ShiftAction>().get());

  // send result to LLVM
  return result;
}
