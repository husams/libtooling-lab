# Native database fixture

`facts.sqlite` and `project.sqlite` are generated from `source.cpp` by the
repository's native facts-tool, never by test SQL. Regenerate from the
facts-tool root after building the executable:

```console
uv run python python/tests/fixtures/native/generate.py \
  ./build-s020/facts-tool /opt/homebrew/opt/llvm/bin/clang++
```

The pair was generated from commit `f4db23c70493315621e23a32fc4ea83cfd882d67`
with facts schema `user_version=10` and LLVM 22 SymbolKind values.
