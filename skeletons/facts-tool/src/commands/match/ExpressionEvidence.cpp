#include "commands/match/ExpressionEvidence.h"

#include "ast/extractors/File.h"
#include "ast/extractors/Location.h"
#include "commands/match/SymbolDispatch.h"
#include "storage/FactStore.h"
#include "storage/Sqlite.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/Expr.h>
#include <clang/AST/ExprCXX.h>
#include <clang/AST/ParentMapContext.h>
#include <clang/AST/StmtCXX.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Lex/Lexer.h>
#include <llvm/Support/SHA256.h>

#include <array>
#include <cstdint>
#include <string_view>

namespace facts::commands::match {
namespace {

std::string hex(std::array<std::uint8_t, 32> bytes) {
  constexpr char digits[] = "0123456789abcdef";
  std::string result;
  result.reserve(64);
  for (const auto byte : bytes) {
    result += digits[byte >> 4U];
    result += digits[byte & 0x0fU];
  }
  return result;
}

std::string fingerprint(clang::FileID file, const clang::SourceManager &source,
                        SourceFingerprintCache &cache) {
  const auto mainEntry = source.getFileEntryRefForID(source.getMainFileID());
  const auto fileEntry = source.getFileEntryRefForID(file);
  const std::string mainName = mainEntry ? mainEntry->getName().str() : "<main>";
  const std::string fileName = fileEntry ? fileEntry->getName().str() : "<unnamed>";
  const std::string key = mainName + "\n" + fileName;
  if (auto found = cache.find(key); found != cache.end())
    return found->second;
  bool invalid = false;
  const auto buffer = source.getBufferData(file, &invalid);
  if (invalid)
    return {};
  llvm::SHA256 hash;
  hash.update(buffer);
  auto value = hex(hash.final());
  cache.emplace(key, value);
  return value;
}

std::string packed(std::optional<SymbolId> id) {
  return id ? std::to_string(storage::packSymbolId(*id)) : "0";
}

std::string sourceScope(const clang::SourceManager &source,
                        clang::SourceLocation location) {
  const auto mainEntry = source.getFileEntryRefForID(source.getMainFileID());
  std::string scope = mainEntry ? mainEntry->getName().str() : "<main>";
  if (scope.empty())
    scope = "<main>";
  if (!location.isInvalid()) {
    const auto file = source.getFileID(source.getExpansionLoc(location));
    const auto entry = source.getFileEntryRefForID(file);
    scope += ":";
    scope += entry ? entry->getName().str() : "<unnamed>";
  }
  return scope;
}

const clang::FunctionDecl *nearestOwner(const clang::Expr &expression,
                                        clang::ASTContext &context) {
  const clang::Stmt *current = &expression;
  for (unsigned depth = 0; depth != 256 && current; ++depth) {
    const auto parents = context.getParents(*current);
    if (parents.empty())
      return nullptr;
    for (const auto &parent : parents) {
      if (const auto *function = parent.get<clang::FunctionDecl>())
        return function;
      if (const auto *statement = parent.get<clang::Stmt>()) {
        current = statement;
        break;
      }
    }
  }
  return nullptr;
}

const clang::NamedDecl *referencedTarget(const clang::Expr &expression) {
  const auto *unwrapped = expression.IgnoreParenImpCasts();
  if (const auto *member = llvm::dyn_cast<clang::MemberExpr>(unwrapped))
    return member->getMemberDecl();
  if (const auto *reference = llvm::dyn_cast<clang::DeclRefExpr>(unwrapped))
    return reference->getDecl();
  return nullptr;
}

bool isSameExpr(const clang::Expr *left, const clang::Expr &right) {
  return left && left->IgnoreParenImpCasts() == right.IgnoreParenImpCasts();
}

bool isTransparent(const clang::Stmt &statement) {
  return llvm::isa<clang::ParenExpr, clang::ImplicitCastExpr,
                   clang::MaterializeTemporaryExpr, clang::CXXBindTemporaryExpr,
                   clang::ExprWithCleanups, clang::ConstantExpr>(&statement);
}

std::string classifyCallArgument(const clang::CallExpr &call,
                                 const clang::Expr &expression,
                                 std::optional<std::string> &reason) {
  unsigned argument = call.getNumArgs();
  for (unsigned index = 0; index != call.getNumArgs(); ++index) {
    if (isSameExpr(call.getArg(index), expression)) {
      argument = index;
      break;
    }
  }
  if (argument == call.getNumArgs()) {
    reason = "call argument effect is unresolved";
    return "unknown";
  }
  const auto *callee = call.getDirectCallee();
  if (!callee || argument >= callee->getNumParams()) {
    reason = "indirect or unresolved call argument effect";
    return "unknown";
  }
  const auto type = callee->getParamDecl(argument)->getType();
  if (type->isLValueReferenceType() || type->isRValueReferenceType())
    return type->getPointeeType().isConstQualified() ? "read" : "escape";
  return "read";
}

std::string classifyAccess(const clang::Expr &expression,
                           clang::ASTContext &context,
                           const clang::NamedDecl *target,
                           std::optional<std::string> &reason) {
  if (!target || !llvm::isa<clang::FieldDecl>(target))
    return "none";

  const clang::Stmt *current = &expression;
  for (unsigned depth = 0; depth != 64 && current; ++depth) {
    const auto parents = context.getParents(*current);
    if (parents.empty())
      break;
    bool movedThroughWrapper = false;
    for (const auto &parent : parents) {
      if (const auto *statement = parent.get<clang::Stmt>()) {
        if (isTransparent(*statement)) {
          current = statement;
          movedThroughWrapper = true;
          break;
        }
        if (const auto *binary = parent.get<clang::BinaryOperator>()) {
          if (binary->isAssignmentOp() &&
              isSameExpr(binary->getLHS()->IgnoreParenImpCasts(), expression))
            return binary->isCompoundAssignmentOp() ? "read_write" : "write";
        }
        if (const auto *unary = parent.get<clang::UnaryOperator>()) {
          if (unary->isIncrementDecrementOp())
            return "read_write";
          if (unary->getOpcode() == clang::UO_AddrOf)
            return "escape";
        }
        if (const auto *call = parent.get<clang::CallExpr>())
          return classifyCallArgument(*call, expression, reason);
      }
    }
    if (movedThroughWrapper)
      continue;
    break;
  }
  return "read";
}

std::string expressionIdentity(const ExpressionOccurrence &value) {
  return value.sourceSha256 + ":" + std::to_string(value.file) + ":" +
         std::to_string(value.offset) + ":" + std::to_string(value.size) +
         ":" + value.expressionKind + ":" + packed(value.owner) + ":" +
         packed(value.target) + ":" + value.access;
}

std::string regionIdentity(const SourceRegion &value) {
  return value.sourceSha256 + ":" + std::to_string(value.file) + ":" +
         std::to_string(value.offset) + ":" + std::to_string(value.size) +
         ":" + std::to_string(storage::packSymbolId(value.symbol)) + ":" +
         value.symbolKind;
}

std::string symbolKind(const clang::NamedDecl &node) {
  if (llvm::isa<clang::CXXMethodDecl>(node))
    return "method";
  if (llvm::isa<clang::FunctionDecl>(node))
    return "function";
  if (llvm::isa<clang::CXXRecordDecl>(node))
    return "record";
  return {};
}

const clang::Decl *definitionNode(const clang::NamedDecl &node) {
  if (const auto *function = llvm::dyn_cast<clang::FunctionDecl>(&node))
    return function->isThisDeclarationADefinition() ? function
                                                     : function->getDefinition();
  if (const auto *record = llvm::dyn_cast<clang::CXXRecordDecl>(&node))
    return record->isThisDeclarationADefinition() ? record
                                                   : record->getDefinition();
  return nullptr;
}

std::expected<SourceRegion, std::string>
makeRegion(const clang::NamedDecl &node, SymbolId id, clang::ASTContext &context,
           FileManager &files, SourceFingerprintCache &fingerprints) {
  SourceRegion result{.symbol = id, .symbolKind = symbolKind(node)};
  const auto *definition = definitionNode(node);
  if (!definition)
    result.unavailableReason = "declaration-only definition";
  if (node.isImplicit())
    result.unavailableReason = "implicit declaration has no source region";
  if (definition && (definition->getBeginLoc().isMacroID() ||
                     definition->getEndLoc().isMacroID()))
    result.unavailableReason = "macro definition has no stable source region";
  if (result.symbolKind.empty())
    result.unavailableReason = "source capture supports function, method, record";

  if (result.unavailableReason) {
    result.freshness = "unavailable";
    result.identity = "unavailable:" + std::to_string(storage::packSymbolId(id)) +
                      ":" + result.unavailableReason.value();
    return result;
  }

  auto location = extractLocation(context.getSourceManager(),
                                  definition->getBeginLoc());
  auto region = extractRegion(context.getSourceManager(),
                              context.getLangOpts(), definition->getSourceRange());
  auto file = resolveFile(context.getSourceManager(), definition->getBeginLoc(), files);
  if (!location || !region || !file || *file == builtinFileId) {
    result.freshness = "unavailable";
    result.unavailableReason = "definition source range is unavailable";
    result.identity = "unavailable:" + std::to_string(storage::packSymbolId(id)) +
                      ":definition";
    return result;
  }
  const auto sourceFile = context.getSourceManager().getFileID(
      context.getSourceManager().getExpansionLoc(definition->getBeginLoc()));
  result.file = *file;
  result.line = location->line;
  result.column = location->column;
  result.offset = region->offset;
  result.size = region->size;
  result.sourceSha256 = fingerprint(sourceFile, context.getSourceManager(), fingerprints);
  if (result.sourceSha256.empty()) {
    result.freshness = "unavailable";
    result.unavailableReason = "definition source buffer is unavailable";
    result.identity = "unavailable:" + std::to_string(storage::packSymbolId(id)) +
                      ":buffer";
    return result;
  }
  result.identity = regionIdentity(result);
  return result;
}

} // namespace

std::expected<void, std::string>
captureExpression(const clang::Expr &expression, clang::ASTContext &context,
                  FileManager &files, FactStore &store,
                  SourceFingerprintCache &fingerprints) {
  ExpressionOccurrence result{.expressionKind = expression.getStmtClassName()};
  const clang::NamedDecl *targetDecl = nullptr;
  const clang::FunctionDecl *ownerDecl = nullptr;

  if (expression.getExprLoc().isInvalid() || expression.getExprLoc().isMacroID() ||
      expression.getSourceRange().getBegin().isMacroID() ||
      expression.getSourceRange().getEnd().isMacroID()) {
    result.freshness = "unavailable";
    result.unavailableReason = "macro or invalid expression source range";
  } else if (context.getSourceManager().isInSystemHeader(expression.getExprLoc())) {
    result.freshness = "unavailable";
    result.unavailableReason = "system-header expression source is outside capture scope";
  } else {
    auto location = extractLocation(context.getSourceManager(), expression.getExprLoc());
    auto region = extractRegion(context.getSourceManager(), context.getLangOpts(),
                                expression.getSourceRange());
    auto file = resolveFile(context.getSourceManager(), expression.getExprLoc(), files);
    if (!location || !region || !file || *file == builtinFileId) {
      result.freshness = "unavailable";
      result.unavailableReason = "expression source range is unavailable";
    } else {
      const auto sourceFile = context.getSourceManager().getFileID(
          context.getSourceManager().getExpansionLoc(expression.getExprLoc()));
      result.file = *file;
      result.line = location->line;
      result.column = location->column;
      result.offset = region->offset;
      result.size = region->size;
      result.sourceSha256 = fingerprint(sourceFile, context.getSourceManager(), fingerprints);
      if (result.sourceSha256.empty()) {
        result.freshness = "unavailable";
        result.unavailableReason = "expression source buffer is unavailable";
      }
    }
  }

  if (result.freshness == "current") {
    targetDecl = referencedTarget(expression);
    std::optional<std::string> reason;
    result.access = classifyAccess(expression, context, targetDecl, reason);
    result.unavailableReason = reason;
    if (expression.isTypeDependent() || expression.isValueDependent() ||
        expression.isInstantiationDependent()) {
      result.access = "unknown";
      result.unavailableReason = "dependent expression effect is unresolved";
    }
    ownerDecl = nearestOwner(expression, context);
  } else {
    result.access = "unknown";
  }

  if (result.freshness == "current" && targetDecl) {
    auto target = persistSymbol(*targetDecl, context, files, store);
    if (!target)
      return std::unexpected("cannot persist expression target: " +
                             target.error());
    result.target = target->id;
  }
  if (result.freshness == "current" && ownerDecl) {
    auto owner = persistSymbol(*ownerDecl, context, files, store);
    if (!owner)
      return std::unexpected("cannot persist expression owner: " +
                             owner.error());
    result.owner = owner->id;
  }
  result.identity = result.freshness == "unavailable"
                        ? "unavailable:" +
                              sourceScope(context.getSourceManager(),
                                          expression.getExprLoc()) + ":" +
                              std::to_string(expression.getExprLoc().getRawEncoding()) +
                              ":" + result.expressionKind
                        : expressionIdentity(result);
  const std::array values{result};
  return store.addExpressionOccurrences(values)
      .transform_error([](std::error_code error) { return error.message(); });
}

std::expected<void, std::string>
captureSourceRegion(const clang::NamedDecl &symbol, SymbolId id,
                    clang::ASTContext &context, FileManager &files,
                    FactStore &store, SourceFingerprintCache &fingerprints) {
  auto region = makeRegion(symbol, id, context, files, fingerprints);
  if (!region)
    return std::unexpected(region.error());
  const std::array values{*region};
  return store.addSourceRegions(values)
      .transform_error([](std::error_code error) { return error.message(); });
}

} // namespace facts::commands::match
