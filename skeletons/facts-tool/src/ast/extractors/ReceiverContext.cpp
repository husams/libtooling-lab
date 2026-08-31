#include "ast/extractors/ReceiverContext.h"

#include "ast/extractors/NamedDecl.h"
#include "ast/extractors/TargetResolution.h"

#include <clang/AST/Decl.h>
#include <clang/AST/ExprCXX.h>
#include <llvm/Support/Casting.h>

namespace facts {
namespace {

const clang::CXXRecordDecl *provenValueReceiver(const clang::Expr &object) {
  const auto *reference =
      llvm::dyn_cast<clang::DeclRefExpr>(object.IgnoreParenImpCasts());
  const auto *variable =
      reference ? llvm::dyn_cast<clang::VarDecl>(reference->getDecl())
                : nullptr;
  if (!variable || variable->getType()->isPointerType() ||
      variable->getType()->isReferenceType())
    return nullptr;
  return variable->getType()->getAsCXXRecordDecl();
}

const clang::CXXRecordDecl *receiverRecord(const clang::Expr &object) {
  auto type = object.getType();
  if (type->isPointerType())
    type = type->getPointeeType();
  return type->getAsCXXRecordDecl();
}

} // namespace

ExtractionResult<ReceiverContext>
extractReceiverContext(const clang::Expr &site,
                       const clang::SourceManager &sourceManager,
                       FileManager &files, FactStore &store) {
  const auto *call = llvm::dyn_cast<clang::CXXMemberCallExpr>(&site);
  if (!call || !call->getImplicitObjectArgument())
    return ReceiverContext{};
  const auto &object = *call->getImplicitObjectArgument();
  const auto *record = receiverRecord(object);
  const auto *exactRecord = provenValueReceiver(object);
  if (!record || !exactRecord)
    return ReceiverContext{record, std::nullopt, ReceiverCertainty::Possible};
  return extractUsr(*exactRecord).and_then([&](std::string usr) {
    return findOrStoreSymbolTarget(*exactRecord, sourceManager, files, store,
                                   usr)
        .transform_error(
            [](std::error_code) { return ExtractionError::InvalidUsr; })
        .transform([exactRecord](SymbolId id) {
          return ReceiverContext{exactRecord, id, ReceiverCertainty::Exact};
        });
  });
}

} // namespace facts
