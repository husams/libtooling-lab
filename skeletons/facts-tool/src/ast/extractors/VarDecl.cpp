#include "ast/extractors/VarDecl.h"

#include "ast/StoreExtracted.h"
#include "ast/extractors/Definition.h"
#include "ast/extractors/Initializer.h"
#include "ast/extractors/NamedDecl.h"
#include "ast/extractors/ValueDecl.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/Attr.h>
#include <clang/AST/Decl.h>

#include <cstdint>
#include <utility>

namespace facts {
namespace {

std::uint32_t flagWhen(SymbolBit flag, bool condition) {
  return condition ? bit(flag) : 0U;
}

bool supportedVariable(const clang::VarDecl &node) {
  return node.isFileVarDecl() || node.isStaticDataMember();
}

Variable addVariableDetails(Variable variable, const clang::VarDecl &node,
                            const clang::SourceManager &sourceManager) {
  const auto constantEvaluation = node.isConstexpr()                     ? 1U
                                  : node.hasAttr<clang::ConstInitAttr>() ? 3U
                                                                         : 0U;
  const auto constexprFlags = constantEvaluation << constexprShift;
  variable.flags =
      (variable.flags & ~(accessMask | (constexprMask << constexprShift))) |
      static_cast<std::uint32_t>(node.getAccess()) |
      flagWhen(StaticBit, node.getStorageClass() == clang::SC_Static ||
                              node.isStaticDataMember()) |
      flagWhen(ConstBit, node.getType().isConstQualified()) |
      flagWhen(InlineBit, node.isInline()) |
      flagWhen(InternalLinkageBit,
               node.getFormalLinkage() == clang::Linkage::Internal) |
      flagWhen(ExternStorageBit, node.getStorageClass() == clang::SC_Extern) |
      constexprFlags;
  variable.initializer = extractInitializer(
      node.getInit(), node.getType(), node.getASTContext(), sourceManager);
  return variable;
}

} // namespace

ExtractionResult<Variable>
extractVariable(const clang::VarDecl &node,
                const clang::SourceManager &sourceManager) {
  const auto toVariable = [](Symbol symbol) -> ExtractionResult<Variable> {
    return toSymbolModel<Variable>(std::move(symbol));
  };
  const auto addDetails = [&](Variable variable) {
    return addVariableDetails(std::move(variable), node, sourceManager);
  };
  const auto addDefinition = [&](Variable variable) {
    return addDefinitionRegion(std::move(variable), node,
                               node.isThisDeclarationADefinition() !=
                                   clang::VarDecl::DeclarationOnly,
                               sourceManager);
  };

  return extractSymbol<Symbol, clang::NamedDecl>(node, sourceManager)
      .and_then(toVariable)
      .transform(addDetails)
      .and_then(addDefinition);
}

IndexingResult collectSymbol(clang::VarDecl &node, clang::ASTContext &context,
                             FileManager &files, FactStore &store) {
  if (!supportedVariable(node)) {
    return {};
  }

  const auto storeRelations = [&](SymbolId variable) {
    return storeValueRelations(node, variable, store);
  };
  return storeExtracted(node, extractVariable(node, context.getSourceManager()),
                        context, files, store, storeRelations);
}

} // namespace facts
