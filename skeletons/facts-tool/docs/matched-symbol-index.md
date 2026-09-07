# Matched-symbol index

`facts-tool match` records only successfully bound `symbol` declarations in
the project database's `matched_symbol_index`. Extraction never populates,
backfills, refreshes, or removes these rows.

The table contains exactly four data columns: `usr`, `qualified_name`,
`file_id`, and the raw Clang index `kind`. Its key is `(usr,file_id)`, so a
repeat match updates name and kind without duplication, while one USR can
retain candidates from multiple registered files. Repository, component, and
path are joined from the existing project catalog when results are displayed.

## Find candidates

```console
facts-tool symbol find --conf project.sqlite --usr 'c:@F@run#'
facts-tool symbol find --conf project.sqlite --name 'app::run' --kind 13
facts-tool symbol find --conf project.sqlite --name '%' --format json
```

Choose exactly one selector. `--usr` is exact; `--name` is a case-sensitive
non-empty literal substring, so `%` and `_` are ordinary characters. All compatible
registered repositories and components are searched by default, and results
are ordered by USR then file ID.

An empty result exits successfully. It means only that no successful prior
match recorded that candidate: it does not prove the symbol is absent from
source. Likewise, an index row does not prove that a definition, body, calls,
or a complete call graph is available. JSON output makes this explicit with
`coverage.index_scope="matched-only"` and `source_complete=null`.

## Clear and rematch

```console
facts-tool symbol index clear --conf project.sqlite --file-id 42
facts-tool match --conf project.sqlite --facts facts.sqlite \
  --matcher 'functionDecl(hasName("app::run")).bind("symbol")' src/run.cpp
```

Ordinary matching is additive and never erases unrelated candidates. Use an
explicit clear followed by a successful rematch when replacement semantics are
required. Clearing an unknown positive file ID is a successful no-op. Removing
a file through the native catalog command cascades its
candidate rows; extraction does not remove file identities to maintain this
index.

Project databases created before schema version 1 need one `facts-tool import`
run before extract, match, lookup, or clear; the import migrates in place.

For separate stores, a successful match commits facts before one project-index
transaction. An index write failure reports
`facts_committed:true,index_committed:false`; retrying is safe. A combined
store publishes facts and index rows in one transaction.
