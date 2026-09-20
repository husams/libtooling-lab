#pragma once
#include <clang/ASTMatchers/ASTMatchers.h>
#include <string_view>

namespace facts::commands::match {
using BindingMap = clang::ast_matchers::BoundNodes::IDToNodeMap;
BindingMap visibleBindings(const clang::ast_matchers::BoundNodes &nodes,
                            std::string_view internalRoot);
}
