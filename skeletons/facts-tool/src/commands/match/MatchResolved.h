#pragma once
#include "cli/Options.h"
#include "commands/match/MatchOutput.h"
#include "commands/match/BindingPolicy.h"
#include <expected>

namespace facts::commands {
std::expected<match::MatchOutput, std::string>
runMatchResolved(const cli::MatchOptions &options, bool installSignals = true,
                  match::BindingPolicy policy = match::BindingPolicy::Contract);
}
