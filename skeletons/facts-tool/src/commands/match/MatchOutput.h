#pragma once
#include <llvm/Support/JSON.h>
#include <string>

namespace facts::commands::match {
// Owned values survive destruction of Clang ASTs and the facts transaction.
struct MatchOutput {
  llvm::json::Object document;
  std::string text;
};
}
