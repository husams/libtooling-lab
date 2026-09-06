#pragma once

#include "cli/Options.h"
#include "model/MatchedSymbol.h"

#include <expected>
#include <optional>
#include <span>
#include <string>

namespace facts {
class FactStore;
}

namespace facts::commands::match {

std::expected<int, std::string>
finishMatch(FactStore &store, const cli::MatchOptions &options, int status,
            std::optional<std::string> error,
            std::span<const MatchedSymbol> symbols);

} // namespace facts::commands::match
