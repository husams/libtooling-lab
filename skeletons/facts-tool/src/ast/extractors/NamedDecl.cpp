#include "ast/extractors/NamedDecl.h"

#include "ast/extractors/Location.h"
#include "ast/extractors/TemplatePattern.h"
#include "ast/extractors/Type.h"

#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/Index/IndexSymbol.h"
#include "clang/Index/USRGeneration.h"

#include <llvm/ADT/SmallString.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/raw_ostream.h>

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace facts {
namespace {

const clang::CXXRecordDecl *asLambda(const clang::Decl *decl) {
  const auto *record = llvm::dyn_cast_or_null<clang::CXXRecordDecl>(decl);
  return record != nullptr && record->isLambda() ? record : nullptr;
}

// Clang's spelling of a lambda is stable neither across releases nor across
// machines: as a scope it prints "(lambda)" from LLVM 22 on and "(anonymous
// class)" before, and the closure type itself prints as "(lambda at
// /abs/path.hpp:50:17)". Name a lambda after the coordinates it was written at
// instead. The file is left out because SymbolId already carries it.
std::string lambdaScope(const clang::CXXRecordDecl &closure,
                        const clang::SourceManager &sourceManager) {
  const auto location = extractLocation(sourceManager, closure.getLocation());
  if (!location) {
    return {};
  }
  return "<lambda@" + std::to_string(location->line) + ":" +
         std::to_string(location->column) + ">";
}

// The enclosing declarations Clang parenthesizes because they have no name of
// their own — unnamed namespaces and unnamed records, a lambda closure among
// them — outermost first, the order a qualified name spells them in. Unnamed
// enums are absent on purpose: Clang skips an unscoped one instead of printing
// it, so listing them here would misalign the scopes with the spelling.
std::vector<const clang::Decl *> unnamedScopes(const clang::NamedDecl &node) {
  std::vector<const clang::Decl *> scopes;
  for (const clang::DeclContext *context = node.getDeclContext();
       context != nullptr; context = context->getParent()) {
    const auto *named = llvm::dyn_cast<clang::NamedDecl>(context);
    if (named != nullptr && named->getIdentifier() == nullptr &&
        llvm::isa<clang::NamespaceDecl, clang::RecordDecl>(named)) {
      scopes.push_back(named);
    }
  }
  std::reverse(scopes.begin(), scopes.end());
  return scopes;
}

// Where each parenthesized scope sits in `spelling`, as (offset, length).
std::vector<std::pair<std::size_t, std::size_t>>
parenthesizedScopes(std::string_view spelling) {
  std::vector<std::pair<std::size_t, std::size_t>> found;
  for (std::size_t at = 0; at < spelling.size();) {
    const std::size_t open = spelling.find('(', at);
    if (open == std::string_view::npos) {
      break;
    }
    const std::size_t close = spelling.find(')', open);
    if (close == std::string_view::npos) {
      break;
    }
    const std::string_view inside = spelling.substr(open + 1, close - open - 1);
    if (inside == "lambda" || inside.starts_with("anonymous ")) {
      found.emplace_back(open, close - open + 1);
    }
    at = close + 1;
  }
  return found;
}

// Rewrites the lambda scopes in a nested-name-specifier. Fails when the
// spelling and the declaration chain disagree about how many parenthesized
// scopes there are — the shapes Clang prints are not exhaustively known here,
// and a name Clang spelled itself beats one rebuilt from a wrong assumption.
std::optional<std::string>
rewriteLambdaScopes(std::string_view spelling,
                    const std::vector<const clang::Decl *> &scopes,
                    const clang::SourceManager &sourceManager) {
  const auto found = parenthesizedScopes(spelling);
  if (found.size() != scopes.size()) {
    return std::nullopt;
  }
  std::string rewritten;
  std::size_t copied = 0;
  for (std::size_t index = 0; index < found.size(); ++index) {
    const auto *closure = asLambda(scopes[index]);
    if (closure == nullptr) {
      continue;
    }
    std::string tag = lambdaScope(*closure, sourceManager);
    if (tag.empty()) {
      return std::nullopt;
    }
    const auto [offset, length] = found[index];
    rewritten.append(spelling.substr(copied, offset - copied));
    rewritten.append(std::move(tag));
    copied = offset + length;
  }
  rewritten.append(spelling.substr(copied));
  return rewritten;
}

// Clang gives every lambda in one function the same USR — it ends in "@Sa"
// with nothing to tell two closures apart — so the second lambda would take
// over the first one's identity, and its call operator and relations with it.
// The source coordinates that name the closure disambiguate it here too.
std::string lambdaDiscriminator(const clang::NamedDecl &node,
                                const clang::SourceManager &sourceManager) {
  std::vector<const clang::Decl *> closures = unnamedScopes(node);
  closures.push_back(&node);
  std::string discriminator;
  for (const clang::Decl *scope : closures) {
    const auto *closure = asLambda(scope);
    if (closure == nullptr) {
      continue;
    }
    const auto location =
        extractLocation(sourceManager, closure->getLocation());
    if (!location) {
      continue;
    }
    discriminator += "@" + std::to_string(location->line) + ":" +
                     std::to_string(location->column);
  }
  return discriminator;
}

} // namespace

std::string extractQualifiedName(const clang::NamedDecl &node,
                                 const clang::SourceManager &sourceManager) {
  std::string spelled = node.getQualifiedNameAsString();
  const auto *closure = asLambda(&node);
  const std::vector<const clang::Decl *> scopes = unnamedScopes(node);
  const bool involvesLambda =
      closure != nullptr ||
      std::any_of(scopes.begin(), scopes.end(), [](const clang::Decl *scope) {
        return asLambda(scope) != nullptr;
      });
  if (!involvesLambda) {
    return spelled;
  }

  std::string prefix;
  llvm::raw_string_ostream stream(prefix);
  node.printNestedNameSpecifier(stream,
                                node.getASTContext().getPrintingPolicy());
  stream.flush();
  // printQualifiedName is printNestedNameSpecifier followed by the leaf name,
  // so the leaf is whatever the prefix does not cover.
  if (!std::string_view{spelled}.starts_with(prefix)) {
    return spelled;
  }
  std::string leaf = spelled.substr(prefix.size());
  if (closure != nullptr) {
    std::string tag = lambdaScope(*closure, sourceManager);
    if (tag.empty()) {
      return spelled;
    }
    leaf = std::move(tag);
  }

  auto rewritten = rewriteLambdaScopes(prefix, scopes, sourceManager);
  if (!rewritten) {
    return spelled;
  }
  return std::move(*rewritten) + leaf;
}

ExtractionResult<std::string> extractUsr(const clang::NamedDecl &node) {
  llvm::SmallString<128> usr;
  if (clang::index::generateUSRForDecl(&node, usr)) {
    return std::unexpected(ExtractionError::InvalidUsr);
  }
  return std::string{usr} +
         lambdaDiscriminator(node, node.getASTContext().getSourceManager());
}

TypeResult extractAliasTarget(const clang::TypedefNameDecl &node,
                              const clang::SourceManager &sourceManager,
                              FileManager &files, FactStore &store) {
  return extractType(node.getUnderlyingType(), sourceManager, files, store);
}

DetailedExtractionResult<std::vector<TemplateArgument>>
extractAliasTemplateArguments(const clang::TypedefNameDecl &node,
                              const clang::SourceManager &sourceManager,
                              FileManager &files, FactStore &store) {
  const auto *alias = llvm::dyn_cast<clang::TypeAliasDecl>(&node);
  if (alias == nullptr || alias->getDescribedAliasTemplate() == nullptr) {
    return std::vector<TemplateArgument>{};
  }
  return extractTemplateArguments(
      *alias->getDescribedAliasTemplate()->getTemplateParameters(),
      sourceManager, files, store);
}

template <>
ExtractionResult<Symbol> extractSymbol<Symbol, clang::NamedDecl>(
    const clang::NamedDecl &node, const clang::SourceManager &sourceManager) {
  const auto toSymbol = [&](Location location) {
    return extractUsr(node).transform([&](std::string usr) {
      Symbol symbol;
      static_cast<clang::index::SymbolInfo &>(symbol) =
          clang::index::getSymbolInfo(&node);
      symbol.usr = std::move(usr);
      symbol.qualifiedName = extractQualifiedName(node, sourceManager);
      symbol.loc = location;
      return symbol;
    });
  };

  return extractLocation(sourceManager, node.getLocation()) | toSymbol;
}

} // namespace facts
