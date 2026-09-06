# Recover missing call evidence

Request recovery explicitly on the native graph command:

```sh
facts-tool analyse call-graph --conf project.sqlite --facts facts.sqlite \
  --function 'app::run' --recover-missing --format json
```

The ordinary command remains read-only when `--recover-missing` is omitted.
Recovery uses the selected project/facts pair and registered translation units,
including other registered components. Register their real compiler commands
before requesting recovery; recovery does not invent flags or header ownership.

The match-only index provides candidates by canonical USR. An absent index hit
does not prove that a definition is unavailable: registered compiler commands
remain discovery candidates. A symbol-only match establishes identity and a
location, while full extraction supplies body and call evidence. Existing usable
facts are reused, and extraction does not maintain the match-only index.
Recovery adds no graph analysis or maintenance to ordinary `extract`; its new
work is entered only through the explicit recovery request. This change builds
on the existing S-027 evidence and does not remove its extraction hooks; their
separate correction must preserve usable ordinary-fact evidence for recovery.

The graph's `recovery` object contains `requested`, `attempted`, `reused`,
`failed`, and `suppressed`. Candidate entries identify the TU, component, driver,
working directory, ordered arguments, reason, and relevant symbol USRs. Recovery
progress appears on stderr as `recovery-start` and `recovery-complete`, keeping
JSON output usable by machine consumers.

A requested recovery failure exits 1, retains the available partial graph, and
reports `recovery-failed`. A successful search can still leave an unavailable
definition boundary; inspect graph coverage rather than interpreting exit 0 as
proof of complete source coverage.

Retry tracking exists only during one invocation. Its identity includes the
canonical store pair, TU, driver, working directory, effective ordered argv,
registry fingerprint, and SHA-256 input contents. Registered transitive includes
participate in input hashing; incomplete dependency coverage requires the full
registered compiler-input set conservatively. Unchanged failed or no-match
attempts are suppressed. Changed input, configuration, or wanted USRs can permit
a new attempt; facts writes alone do not. A new process starts with no previous
failure memory. Unreadable inputs are reported as gaps.

See [graph coverage](call-graph.md) and [shared entries](call-graph-entries.md)
for the distinction between a stored graph, generation evidence, and freshness.
