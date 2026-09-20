#include "commands/match/VisibleBindings.h"

namespace facts::commands::match {
BindingMap visibleBindings(const clang::ast_matchers::BoundNodes &nodes,
                            std::string_view internalRoot) {
  auto result = nodes.getMap();
  if (internalRoot.empty()) return result;
  auto root = result.extract(std::string(internalRoot));
  if (result.empty() && !root.empty()) {
    root.key() = "root";
    result.insert(std::move(root));
  }
  return result;
}
}
