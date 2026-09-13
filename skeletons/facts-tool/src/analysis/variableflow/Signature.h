#pragma once

#include <clang/AST/Decl.h>

#include <string>
#include <string_view>

namespace facts::variableflow::detail {

std::string functionSignature(const clang::FunctionDecl &,
                              bool canonical = false);
bool matchesFunctionSignature(const clang::FunctionDecl &, std::string_view);
std::string describeFunction(const clang::FunctionDecl &, std::string_view usr);

} // namespace facts::variableflow::detail
