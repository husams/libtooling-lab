# Two-database lifecycle and identity

The facts database stores symbols, fact side tables, relations, sites, and
include dependencies. The project database owns repositories, active clones,
components, directories, files, and compile configurations. Both are required.

Opening resolves existing paths, rejects non-files and the same physical file,
and uses SQLite URI `mode=ro&immutable=1` plus connection-local query-only
mode. The SDK creates no path, table, migration, backfill, journal setting, or
persistent side file. Context exit and exceptional construction close both
connections.

Facts schema `user_version=10` is supported. Required tables validate database
roles independently. Project schemas are capability-checked by required table
shape because their current schema has no `user_version` contract. Missing
FileIds referenced by symbols, definitions, sites, or includes prove a pair
mismatch and fail with `E_DATABASE_PAIR`.

Numeric overlap cannot prove that arbitrary databases came from the same
indexing run. Successful pairs therefore report `pairing="unverifiable"`.
Each result still carries canonical paths, file device/inode/size/mtime and
independent SQLite schema identities. This is provenance, not a claim of an
atomic cross-database snapshot.

`SymbolId` is two unsigned 32-bit halves packed as `(file_id << 32) | index`.
Python round-trips signed SQLite values through the unsigned 64-bit domain.
JSON exports stringify integers beyond JavaScript's exact range. Logical row
IDs include their domain and results include database-pair scope.

Declaration FileId is the symbol ID high half. Definitions and relation sites
carry separate FileIds. FileId 0 is compiler-provided and resolves to no path.
Other IDs join file → directory → component. A relative repository component
anchors under its active clone, then optional version; absolute/ungrouped
components anchor themselves. Missing mappings are errors, never invented
paths.
