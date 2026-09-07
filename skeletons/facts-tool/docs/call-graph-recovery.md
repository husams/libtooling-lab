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
Unknown freshness triggers validation of current definition extents and complete
call occurrences against the stored graph. Validation runs the native extractor
on the retained AST into an isolated temporary facts store, then compares its
canonical graph evidence; it shares the extractor's handling of implicit range
loops, iterator operators, initializer lists, constructors and destructor calls.
The temporary store is removed after collection and never replaces the user's
facts. Its evidence is cached for the candidate within the invocation.
At default verbosity, temporary validation emits no extractor progress or
coverage notices and never reports symbols as recorded into the user's facts.
With `-v 1` or higher, validation identifies its temporary scope and enables
extractor diagnostics at their usual levels; compiler errors remain visible.
Existing non-stale body entries support reuse of
derived virtual-dispatch evidence; unresolved call boundaries remain explicit.
A missing entry alone does not force extraction, while incomplete or changed
evidence requires recovery. Validated entry evidence survives recovery writes.
External-unavailable targets remain coverage gaps without becoming project TU
probes, and `--max-depth` limits both recovery and reused reporting.
Recovery adds no graph analysis or maintenance to ordinary `extract`; its new
work is entered only through the explicit recovery request. This change builds
on the existing S-027 evidence and does not remove its extraction hooks; their
separate correction must preserve usable ordinary-fact evidence for recovery.

The graph's `recovery` object contains `requested`, `attempted`, `reused`,
`failed`, and `suppressed`. Candidate entries identify the TU, component, driver,
working directory, ordered arguments, reason, and relevant symbol USRs. Reused
entries group symbols under a registered owning TU, including header bodies. Recovery
progress appears on stderr as `recovery-start` and `recovery-complete`, keeping
JSON output usable by machine consumers.
The default text format prints the ordinary graph without a recovery summary;
use `--format json` to inspect attempted, reused, failed and suppressed entries.

A requested recovery failure exits 1, retains the available partial graph, and
reports `recovery-failed`. A successful search can still leave an unavailable
definition boundary; inspect graph coverage rather than interpreting exit 0 as
proof of complete source coverage.

Retry tracking exists only during one invocation. Its identity includes the
canonical store pair, TU, driver, working directory, effective ordered argv,
registry fingerprint, and SHA-256 input contents. The candidate's single front-end run supplies its matching definitions, body evidence and
registered transitive input closure; only
incomplete dependency coverage requires the full registered compiler-input set
conservatively. If extraction is needed it consumes that same parsed AST.
Within the invocation, parsed candidates and content hashes are reused after
checking the registry, registered-file availability, file identity and timestamps;
changed inputs invalidate the candidate and its include closure.
Unchanged failed attempts and no-match searches covering the current wanted-USR
subset are suppressed. Changed input, configuration, or added wanted USRs can permit
a new attempt; facts writes alone do not. A new process starts with no previous
failure memory. Unreadable inputs are reported as gaps.

See [graph coverage](call-graph.md) and [shared entries](call-graph-entries.md)
for the distinction between a stored graph, generation evidence, and freshness.
