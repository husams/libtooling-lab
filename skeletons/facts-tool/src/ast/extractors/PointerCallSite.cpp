#include "ast/extractors/PointerCallSite.h"

#include "ast/StoreExtracted.h"
#include "ast/extractors/File.h"
#include "ast/extractors/Initializer.h"
#include "ast/extractors/Location.h"
#include "ast/extractors/NamedDecl.h"
#include "ast/extractors/Reference.h"
#include "ast/extractors/RelationTarget.h"
#include "ast/extractors/VarDecl.h"
#include "storage/FactStore.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/Expr.h>
#include <clang/AST/PrettyPrinter.h>
#include <clang/AST/Type.h>

namespace facts {
namespace {

bool callableType(clang::QualType type) {
  if (type.isNull())
    return false;
  const auto value = type.getNonReferenceType();
  return value->isFunctionPointerType() || value->isFunctionType() ||
         value->isMemberFunctionPointerType();
}

const clang::Expr &pointerOperand(const clang::CallExpr &call) {
  const auto *callee = call.getCallee()->IgnoreParenImpCasts();
  if (const auto *member = llvm::dyn_cast<clang::BinaryOperator>(callee);
      member && (member->getOpcode() == clang::BO_PtrMemD ||
                 member->getOpcode() == clang::BO_PtrMemI))
    return *member->getRHS()->IgnoreParenImpCasts();
  while (const auto *unary = llvm::dyn_cast<clang::UnaryOperator>(callee)) {
    if (unary->getOpcode() != clang::UO_Deref)
      break;
    callee = unary->getSubExpr()->IgnoreParenImpCasts();
  }
  return *callee;
}

const clang::ValueDecl *pointerDeclaration(const clang::Expr &operand) {
  const clang::ValueDecl *decl = nullptr;
  if (const auto *reference = llvm::dyn_cast<clang::DeclRefExpr>(&operand))
    decl = reference->getDecl();
  if (const auto *member = llvm::dyn_cast<clang::MemberExpr>(&operand))
    decl = member->getMemberDecl();
  // An array, a factory, and a conditional selector do not identify one
  // pointer variable. Preserve their typed call site without inventing one.
  return decl && llvm::isa<clang::VarDecl, clang::FieldDecl>(decl) &&
                 callableType(decl->getType())
             ? decl
             : nullptr;
}

ExtractionResult<std::optional<SymbolId>>
storeLocalPointer(const clang::VarDecl &decl,
                  const clang::SourceManager &sourceManager,
                  FileManager &files, FactStore &store) {
  const auto file = resolveFile(sourceManager, decl.getLocation(), files);
  if (!file)
    return std::unexpected(ExtractionError::RelationTarget);
  // Locals and parameters are normally not standalone symbols. Materialize
  // the actual called value with its Clang identity, never as an external
  // function placeholder.
  return extractVariable(decl, sourceManager)
      .and_then([&](Variable symbol) {
        return store.save(*file, std::move(symbol))
            .transform([](SymbolId id) { return std::optional<SymbolId>{id}; })
            .transform_error([](std::error_code) {
              return ExtractionError::RelationTarget;
            });
      })
      .or_else([](ExtractionError error)
                   -> ExtractionResult<std::optional<SymbolId>> {
        if (isFilteredExtraction(error))
          return std::nullopt;
        return std::unexpected(error);
      });
}

ExtractionResult<std::optional<SymbolId>>
pointerTarget(const clang::ValueDecl *decl,
               const clang::SourceManager &sourceManager, FileManager &files,
               FactStore &store) {
  if (!decl)
    return std::nullopt;
  if (const auto *variable = llvm::dyn_cast<clang::VarDecl>(decl);
      variable && variable->isLocalVarDeclOrParm()) {
    const auto usr = extractUsr(*decl);
    if (!usr)
      return std::nullopt;
    return store.findId(*usr)
        .transform_error([](std::error_code) {
          return ExtractionError::RelationTarget;
        })
        .and_then([&](std::optional<SymbolId> id) {
          return id ? ExtractionResult<std::optional<SymbolId>>{id}
                    : storeLocalPointer(*variable, sourceManager, files, store);
        });
  }
  return resolveRelationTarget(*decl, sourceManager, files, store);
}

std::string pointerSignature(const clang::CallExpr &call,
                             const clang::ValueDecl *decl,
                             const clang::ASTContext &context) {
  const auto &operand = pointerOperand(call);
  const auto type = decl ? decl->getType()
                   : callableType(operand.getType()) ? operand.getType()
                                                    : call.getCallee()->getType();
  return type.getCanonicalType().getAsString(context.getPrintingPolicy());
}

} // namespace

bool isPointerCall(const clang::CallExpr &call) {
  return !call.getDirectCallee() &&
         (callableType(call.getCallee()->getType()) ||
          callableType(pointerOperand(call).getType()));
}

ExtractionResult<std::optional<PointerCallSite>>
extractPointerCallSite(const clang::FunctionDecl &caller,
                       const clang::CallExpr &call, clang::ASTContext &context,
                       FileManager &files, FactStore &store) {
  const auto &sourceManager = context.getSourceManager();
  const auto location = extractLocation(sourceManager, call.getExprLoc());
  if (!location)
    return std::unexpected(location.error());
  const auto file = resolveFile(sourceManager, call.getExprLoc(), files);
  if (!file)
    return std::unexpected(ExtractionError::RelationTarget);
  const auto *decl = pointerDeclaration(pointerOperand(call));
  return extractUsr(referenceOwner(caller))
      .and_then([&](const std::string &usr) {
        return store.findId(usr).transform_error([](std::error_code) {
          return ExtractionError::RelationTarget;
        });
      })
      .and_then([&](std::optional<SymbolId> source)
                    -> ExtractionResult<std::optional<PointerCallSite>> {
        if (!source)
          return std::unexpected(ExtractionError::RelationTarget);
        return pointerTarget(decl, sourceManager, files, store)
            .transform([&](std::optional<SymbolId> target)
                           -> std::optional<PointerCallSite> {
              return PointerCallSite{
                  .source = *source,
                  .target = target,
                  .file = *file,
                  .location = *location,
                  .signature = pointerSignature(call, decl, context),
                  .expression = extractExpressionText(*call.getCallee(),
                                                       context, sourceManager)};
            });
      });
}

} // namespace facts
