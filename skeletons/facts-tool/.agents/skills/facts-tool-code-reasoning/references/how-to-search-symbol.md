# How to search for a symbol

Confirm the native executable and configuration with `facts-tool config show`.
Use the project database that registered the source files together with its
facts database. Search through the public native interface:

```sh
facts-tool symbol find --conf project.db --facts facts.db --name main
facts-tool symbol find --conf project.db --facts facts.db --usr 'EXACT_USR'
```

Retain every candidate's USR, qualified name, file identity, and kind. If a name
is ambiguous, select its exact USR; do not choose the first result silently.
Check `facts-tool symbol find --help` for output-format options.

A global-index miss does not prove source absence. Use a targeted native match
on registered source candidates when necessary:

```sh
facts-tool match --conf project.db --facts facts.db \
  --matcher 'functionDecl(hasName("main")).bind("symbol")' source.cpp
```

Only successful matching populates the global matched-symbol index. Ordinary
extraction is not a way to populate that index. Symbol discovery alone does
not establish body, outgoing-call, or source-freshness coverage.

Use the selected name or USR in the
[call-graph workflow](how-to-build-call-graph.md), which reports graph and source
coverage separately. Use native reported source locations for local detail;
do not query database internals or duplicate native traversal through the SDK.
