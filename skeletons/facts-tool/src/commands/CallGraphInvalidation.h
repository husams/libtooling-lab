#pragma once

#include "model/SymbolId.h"

#include <expected>
#include <span>
#include <string>
#include <vector>

namespace facts::config {
struct Resolved;
}

namespace facts::storage {
class Database;
}

namespace facts::commands {

std::expected<bool, std::string>
callGraphProjectHasFiles(const std::string &configuration);

std::expected<std::vector<std::string>, std::string>
configuredCallGraphFactPaths(const config::Resolved &resolved,
                             const std::string &explicitFacts,
                             const std::vector<std::string> &sources);

// Returns whether call-graph entries were actually invalidated (the paired
// facts database existed and was touched); false is a no-op (nothing to
// invalidate), not a failure.
std::expected<bool, std::string>
invalidateCallGraphEntriesBeforeMutation(const std::string &configuration,
                                         const std::string &facts);

// A project mutation (a changed compile command, a removed file, an edited
// compile option) makes whatever facts extraction previously wrote for the
// affected files obsolete, the same as it makes a paired facts database's
// call-graph entries obsolete; this resets every file row's index state
// (indexed, indexed_at, mtime, facts_db, git_commit) so the next plain
// `extract` re-extracts instead of trusting stale bookkeeping. A no-op on a
// project database that does not exist yet, or does not carry the
// index-state columns yet.
//
// Deliberately not folded into invalidateConfiguredCallGraphEntries: that
// runs *before* a mutation is attempted (so a rejected/rolled-back mutation
// never touches the facts side either), but resetting index state ahead of
// an attempt that might fail would corrupt an otherwise-unchanged catalog.
// Call this only after the mutation itself has committed -- or, for the
// overload below, from inside the same transaction as the mutation, so a
// failure here rolls the mutation back too instead of leaving it committed
// with unreset index state.
std::expected<void, std::string> resetIndexState(storage::Database &database);
std::expected<void, std::string>
resetIndexState(const std::string &configuration);

// Same as resetIndexState(storage::Database&), but scoped to exactly the
// given rows (a "file set-option"/"clear-option" mutation only invalidates
// the rows it touched, not the whole table). A no-op for an empty span or a
// database that does not carry the index-state columns yet.
std::expected<void, std::string>
resetIndexStateForIds(storage::Database &database,
                      std::span<const FileId> ids);

// Invalidates the call-graph entries of every facts database configured for
// this project (or the explicit/derived one, when the caller supplies
// sources). Returns whether anything was actually invalidated, so a caller
// can gate a coarser index-state reset on "a paired facts database actually
// existed and was touched" rather than resetting unconditionally.
std::expected<bool, std::string> invalidateConfiguredCallGraphEntries(
    const config::Resolved &resolved, const std::string &explicitFacts,
    const std::vector<std::string> &sources = {});

} // namespace facts::commands
