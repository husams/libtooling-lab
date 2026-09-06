#pragma once

#include "storage/FactProvenance.h"

#include <expected>
#include <span>
#include <string>
#include <vector>

namespace facts {
class FactStore;

namespace commands {

using FactPairProvenanceSnapshot = std::vector<storage::FactProvenance>;

// Performs a read-only pairing check and returns the project identity snapshot
// to register after extraction, while the facts transaction is still active.
std::expected<FactPairProvenanceSnapshot, std::string>
prepareFactPairForWrite(const std::string &factsPath,
                        const std::string &projectPath);

// Registers only fact-used files and explicitly selected inputs in the active
// FactStore transaction; failure leaves that transaction available to roll
// back.
std::expected<void, std::string>
registerFactPairProvenance(FactStore &store,
                           const FactPairProvenanceSnapshot &snapshot,
                           std::span<const FileId> selected = {});

// Validate a read-only facts/project pairing.  A legacy facts store may have
// no provenance rows; callers must present that state as unknown.
std::expected<void, std::string>
validateFactPairForRead(const std::string &factsPath,
                        const std::string &projectPath);

// Validate and register the project file identities before facts are written.
// Existing rows are never rewritten; an incompatible row rejects the pair.
std::expected<void, std::string>
validateFactPairForWrite(const std::string &factsPath,
                         const std::string &projectPath);

// True when existing facts contain symbols but no provenance registration.
// Callers may allow a zero-match operation while rejecting its first write.
std::expected<bool, std::string>
legacyFactsNeedRegistration(const std::string &factsPath,
                            const std::string &projectPath);

} // namespace commands
} // namespace facts
