#pragma once

#include "commands/catalog/MatchedSymbolData.h"

#include <string>
#include <vector>

namespace facts::commands {

std::string
renderMatchedSymbolsText(const std::vector<MatchedSymbolCandidate> &values);
std::string
renderMatchedSymbolsJson(const std::vector<MatchedSymbolCandidate> &values);

} // namespace facts::commands
