# Native database fixture

`facts.sqlite` and `project.sqlite` are generated from `source.cpp` by the
repository's native facts-tool, never by test SQL. Regenerate from the
facts-tool root after building the executable:

```console
uv run python python/tests/fixtures/native/generate.py \
  ./build-s020/facts-tool /opt/homebrew/opt/llvm/bin/clang++
```

The checked-in base pair was generated with the native facts-tool executable
built from commit `f4db23c70493315621e23a32fc4ea83cfd882d67`, facts schema
`user_version=10`, and LLVM 22 SymbolKind values. To exercise the current graph
reader, copy the pair and run `facts-tool analyse call-graph -v 0 -f facts.sqlite
-c project.sqlite --function app::run`; native migration then produces schema
12 and an append-only `callgraph_run`. The checked-in source and header are its
complete inputs; the SDK never invokes this generator at runtime.
