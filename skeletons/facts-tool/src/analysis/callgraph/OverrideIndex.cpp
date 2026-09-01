#include "analysis/callgraph/OverrideIndex.h"

#include <algorithm>
#include <clang/AST/DeclCXX.h>
#include <ranges>

namespace facts::callgraph {
namespace {

bool reaches(const std::vector<const OverrideFact *> &facts,
             const clang::CXXMethodDecl &method) {
  const auto *canonical = method.getCanonicalDecl();
  return std::ranges::any_of(facts, [&](const auto *fact) {
    return fact->source->getCanonicalDecl() == canonical;
  });
}

bool appliesTo(const OverrideFact &fact, const clang::CXXRecordDecl &receiver,
               ReceiverCertainty certainty) {
  if (certainty == ReceiverCertainty::Possible)
    return true;
  const auto *owner = fact.owner->getCanonicalDecl();
  const auto *actual = receiver.getCanonicalDecl();
  return owner == actual || actual->isDerivedFrom(owner);
}

bool isOverridden(const OverrideFact &candidate,
                  const std::vector<const OverrideFact *> &facts) {
  const auto *method = candidate.source->getCanonicalDecl();
  return std::ranges::any_of(facts, [&](const auto *other) {
    return other != &candidate &&
           other->destination->getCanonicalDecl() == method;
  });
}

} // namespace

std::vector<DispatchTarget>
OverrideIndex::targets(const clang::CXXMethodDecl &base,
                       const clang::CXXRecordDecl &receiver,
                       ReceiverCertainty certainty, SymbolId baseSymbol) const {
  std::vector<const OverrideFact *> reachable;
  const auto *canonicalBase = base.getCanonicalDecl();
  for (bool changed = true; changed;) {
    changed = false;
    for (const auto &override : overrides_)
      if (!reaches(reachable, *override.source) &&
          (override.destination->getCanonicalDecl() == canonicalBase ||
           reaches(reachable, *override.destination))) {
        reachable.push_back(&override);
        changed = true;
      }
  }
  std::vector<const OverrideFact *> applicable;
  std::ranges::copy_if(
      reachable, std::back_inserter(applicable),
      [&](const auto *fact) { return appliesTo(*fact, receiver, certainty); });
  std::vector<DispatchTarget> targets;
  for (const auto *fact : applicable)
    if (!isOverridden(*fact, applicable))
      targets.push_back({fact->relation.source, fact->source});
  if (targets.empty())
    targets.push_back({baseSymbol, &base});
  return targets;
}

} // namespace facts::callgraph
