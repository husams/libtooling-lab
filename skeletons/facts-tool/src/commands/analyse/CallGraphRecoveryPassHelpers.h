#pragma once

#include "commands/analyse/CallGraphRecoveryInternal.h"

namespace facts::commands {
bool processRecoveryProbe(const RecoveryContext &context,
                          RecoveryCandidate &candidate,
                          const recovery::AttemptKey &key,
                          const recovery::RequestedUsrs &requested,
                          RecoveryReport &report,
                          recovery::AttemptCache &cache);
} // namespace facts::commands
