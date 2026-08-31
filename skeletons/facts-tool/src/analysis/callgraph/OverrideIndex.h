#pragma once

#include "analysis/callgraph/CallGraphTypes.h"
#include "model/ReceiverCertainty.h"

#include <span>
#include <vector>

namespace facts::callgraph {

class OverrideIndex {
public:
  explicit OverrideIndex(std::span<const OverrideFact> overrides)
      : overrides_(overrides) {}

  std::vector<const OverrideFact *>
  targets(const clang::CXXMethodDecl &base,
          const clang::CXXRecordDecl &receiver,
          ReceiverCertainty certainty) const;

private:
  std::span<const OverrideFact> overrides_;
};

} // namespace facts::callgraph
