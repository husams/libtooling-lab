# Troubleshooting

`E_DATABASE` means a path is missing, not a regular file, corrupt, or cannot be
opened read-only. The SDK will not create or repair it. Quote shell paths with
spaces; Python path objects need no special handling.

`E_DATABASE_ROLE` means required tables do not match the supplied role or both
arguments resolve to the same file. Confirm `facts_db` points to extraction
output and `project_db` points to the project/configuration registry.

`E_SCHEMA` reports a facts `user_version` other than 10. Upgrade with the
native facts-tool workflow outside this SDK; query sessions never migrate.

`E_DATABASE_PAIR` names FileIds used by facts but absent from the project
registry. Use the registry paired with extraction. A successful open still
reports pairing as unverifiable because numeric overlap alone is not proof.

`E_SOURCE` can mean absent or ambiguous spelling. Prefer a USR or exact
qualified name. `E_FIELD`, `E_VIEW`, and `E_RELATION` identify catalog typos;
consult [views](views.md) and [relations](relations.md).

`E_CAPABILITY` prevents an apparently complete answer when facts-tool does not
store cidx entity/type-layer/call-argument semantics. Query direct persisted
relations or inspect the compatibility matrix.

When a result is truncated, raise a deliberate budget or paginate. Do not
interpret a truncated empty/count result as proof of absence.
