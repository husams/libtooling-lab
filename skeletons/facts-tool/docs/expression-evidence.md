# Expression and source evidence

Expression evidence is opt-in through the native `match` command. The matcher
must bind exactly `"expression"` to an `Expr`; relation kinds are rejected.
Ordinary extraction does not collect these rows.

| Observed construct | Stored access | Scope |
| --- | --- | --- |
| Direct field assignment left-hand side | `write` | Proven direct effect |
| Compound assignment or increment/decrement | `read_write` | Proven direct effect |
| Value use | `read` | Proven value read |
| Address-of a field | `escape` | Proven pointer escape |
| Direct call through a non-const reference parameter | `escape` | Proven reference escape |
| Indirect or unresolved call argument | `unknown` | Alias effect is unresolved |
| Non-field expression | `none` | No field access target |
| Parentheses and implicit casts | Same as the wrapped field expression | Transparent wrappers are followed |

Alias-mediated assignments are not fabricated. Dependent, macro, system-header,
invalid, and unavailable source ranges are persisted as `unknown` or
`unavailable` with a reason. Constructor member initializers and unsupported
dependent forms remain outside the direct expression capture scope and must be
reported through the matcher result rather than treated as complete coverage.

`--capture-source` captures function, method, and record definitions. Each
`source_region` row stores its own file range and lowercase SHA-256 fingerprint;
each translation unit clears its fingerprint cache. Identities include the
stable file scope, content fingerprint, range, and symbol identity, so changed
source creates a new evidence version and unavailable occurrences from separate
translation units do not collide.
