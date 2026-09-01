#pragma once

#include "analysis/callgraph/CallGraphTypes.h"
#include "model/ReceiverCertainty.h"

#include <span>
#include <vector>

namespace facts::callgraph {

struct DispatchTarget {
  SymbolId symbol;
  const clang::CXXMethodDecl *method = nullptr;
};

class OverrideIndex {
public:
  explicit OverrideIndex(std::span<const OverrideFact> overrides)
      : overrides_(overrides) {}

  std::vector<DispatchTarget> targets(const clang::CXXMethodDecl &base,
                                      const clang::CXXRecordDecl &receiver,
                                      ReceiverCertainty certainty,
                                      SymbolId baseSymbol) const;

private:
  std::span<const OverrideFact> overrides_;
};

} // namespace facts::callgraph
