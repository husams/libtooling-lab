#include "commands/match/MatchContract.h"
#include "commands/match/RelationKinds.h"
#include "commands/match/RelationValidation.h"

#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/Tooling/Tooling.h>

#include <array>
#include <cstdlib>

using namespace clang;
using namespace clang::ast_matchers;
using namespace facts::commands::match;
using facts::RelationKind;

namespace {
template <typename Value>
void require(Value &&value) {
  if (!static_cast<bool>(value))
    std::abort();
}

template <typename Matcher>
const NamedDecl &named(ASTContext &context, const Matcher &matcher) {
  auto found = match(matcher.bind("node"), context);
  require(found.size() == 1);
  return *found.front().template getNodeAs<NamedDecl>("node");
}

void testKinds() {
  constexpr std::array names{"Calls", "Inherits", "Contains", "Overrides",
                             "Uses",  "FieldOf",  "MethodOf"};
  for (const auto *name : names) {
    auto parsed = parseRelationKind(name);
    require(parsed && relationName(*parsed) == name);
  }
  require(!parseRelationKind("Unknown"));
}

void testEndpoints(ASTContext &context) {
  const auto &base = named(context, cxxRecordDecl(hasName("N::Base")));
  const auto &record = named(context, cxxRecordDecl(hasName("N::Record")));
  const auto &field = named(context, fieldDecl(hasName("N::Record::field")));
  const auto &function = named(context, functionDecl(hasName("N::function")));
  const auto &variable = named(context, varDecl(hasName("N::variable")));
  const auto &enumeration = named(context, enumDecl(hasName("N::Colour")));
  const auto &enumerator = named(context, enumConstantDecl(hasName("N::Red")));
  const auto &method = named(context, cxxMethodDecl(hasName("N::Record::run")));
  const auto &space = named(context, namespaceDecl(hasName("N")));
  require(validateEndpoints(RelationKind::Inherits, record, base));
  require(validateEndpoints(RelationKind::Contains, record, field));
  require(validateEndpoints(RelationKind::Contains, enumeration, enumerator));
  require(validateEndpoints(RelationKind::Uses, function, variable));
  require(validateEndpoints(RelationKind::Overrides, method, method));
  require(validateEndpoints(RelationKind::FieldOf, field, record));
  require(validateEndpoints(RelationKind::MethodOf, method, record));
  require(!validateEndpoints(RelationKind::Inherits, function, base));
  require(!validateEndpoints(RelationKind::Contains, function, field));
  require(!validateEndpoints(RelationKind::Uses, record, variable));
  require(!validateEndpoints(RelationKind::Uses, function, space));
}

void testContracts(ASTContext &context) {
  auto overrides =
      match(cxxMethodDecl(hasName("N::Record::run"),
                          forEachOverridden(cxxMethodDecl().bind("target")))
                .bind("source"),
            context);
  require(overrides.size() == 1);
  require(classify(overrides.front(), "Overrides"));
  auto missingSite = classify(overrides.front(), "Uses");
  require(!missingSite && missingSite.error() == "Uses requires site binding");
  auto symbol = match(varDecl(hasName("N::variable")).bind("symbol"), context);
  require(symbol.size() == 1 && classify(symbol.front(), std::nullopt));
  require(!classify(symbol.front(), "Uses"));
}
} // namespace

int main() {
  testKinds();
  auto ast = tooling::buildASTFromCodeWithArgs(
      "namespace N { struct Base { virtual void run(); }; struct Record : Base "
      "{ int field; void run() override; }; enum Colour { Red }; int variable; "
      "void function(); }",
      {"-std=c++23"});
  require(ast);
  testEndpoints(ast->getASTContext());
  testContracts(ast->getASTContext());
}
