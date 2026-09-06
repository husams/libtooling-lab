#pragma once

#include "model/MatchedSymbol.h"

#include <filesystem>
#include <string>

namespace facts::commands {

struct MatchedSymbolCandidate {
  MatchedSymbol symbol;
  std::filesystem::path path;
  std::string component;
  std::string repository;
};

} // namespace facts::commands
