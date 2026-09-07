#pragma once

#include "analysis/callgraph/CallGraphRecoveryTypes.h"
#include "cli/Options.h"
#include "commands/analyse/CallGraphRequest.h"
#include "commands/analyse/CallGraphResult.h"
#include "commands/analyse/CallGraphRunRecord.h"

#include <string>

namespace facts::commands {

// Classifies one finished traversal generation into the run to persist.
// `error` names an operational failure observed after traversal started.
CallGraphRunRecord
makeCallGraphRunRecord(const cli::CallGraphOptions &options,
                       const CallGraphRequest &request,
                       const CallGraphResult &result,
                       const callgraph::RecoveryReport *recovery,
                       bool cancelled, std::string error);

} // namespace facts::commands
