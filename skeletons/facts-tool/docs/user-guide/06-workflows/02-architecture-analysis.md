# Workflow: Architecture Analysis

## Goal

Use the Python SDK to build an architecture overview of a codebase you've
already extracted: counts of key symbol kinds, project-scoped versus
whole-translation-unit counts, the biggest classes by method count,
inheritance, namespaces, and a virtual/override inventory, while avoiding
two traps that will otherwise give you silently wrong or unusably slow
results.

This continues from [Onboarding a Codebase](01-onboarding-a-codebase.md):
the same `project.sqlite`/`facts.sqlite` pair built there, from extracting
`facts-tool`'s own 212-file source tree (15412 symbols recorded), is used
throughout.

## Prerequisites

- An extracted `facts.sqlite`/`project.sqlite` pair (see
  [Onboarding a Codebase](01-onboarding-a-codebase.md)).
- The Python SDK installed (see
  [Getting Started](../05-python-sdk/01-getting-started.md)).

## Steps

### 1. A naive count() hits the default enumeration budget

```python
q = start(codebase()) | nodes(eq("kind", "function")) | count()
r = cb.executor.run(q.plan)
print(r.to_dict())
# {'shape': 'scalar', ..., 'truncated': True, ..., 'scalar': None}
```

**What this tells you:** with the default `Budgets(enumeration=10_000)`, a
`count()` over more than 10,000 enumerated symbols comes back
**truncated with `scalar=None`**, not a wrong number. This codebase's
symbol table (15412 rows, mostly instantiated templates and lambdas)
exceeds that default. Always check `.truncated` before trusting a count on
a large codebase; the fix is to raise the enumeration budget explicitly:

```python
with open_codebase(facts_db=facts, project_db=project,
                    budgets=Budgets(enumeration=30000)) as cb:
    for kind in ("function", "instance_method", "class", "struct",
                 "class_method", "static_method"):
        r = cb.executor.run((start(codebase())|nodes(eq("kind", kind))|count()).plan)
        print(kind, r.scalar, r.truncated)
```

```text
function 1777 False
instance_method 3912 False
class 3184 False
struct 1682 False
class_method 0 False
static_method 157 False
```

### 2. Whole-translation-unit counts versus project-scoped counts

```python
proj_glob = glob("file", "*/facts-tool/src/*")
q = start(codebase()) | nodes(all_of((eq("kind", "class"), proj_glob))) | count()
```

```text
project-owned function: 1381   (of 1777 total)
project-owned instance_method: 2718   (of 3912 total)
project-owned class: 1134   (of 3184 total)
project-owned struct: 216   (of 1682 total)
```

**What this tells you:** `nodes(eq("kind","class"))` counts **every**
`class`-kind symbol Clang saw while parsing any translation unit, including
libc++, Clang, and third-party classes instantiated in headers. Filtering
`file` by a project-path glob is necessary to get project-owned counts.
Even scoped to the project, 1134 "classes" is much larger than the roughly
90 hand-written classes in this codebase; most of the remainder are
template instantiations and lambda closure types, which each get their own
`class`-kind symbol row.

### 3. Biggest classes, bases, and a real performance gotcha

```python
names = sorted({r["name"] for r in rows
                if "(lambda" not in r["name"] and "<" not in r["name"]})
# 45 distinct "real" (non-lambda, non-template-instance) project class names
for name in names[:40]:
    rec = cb.get(name)
    n_methods, n_fields, n_bases = len(list(rec.methods())), len(list(rec.fields())), len(list(rec.bases()))
```

```text
(scanned 39 classes in 116.81s)
== biggest classes by method count ==
  methods= 18 fields=  9 bases=1 BodyVisitor
  methods= 11 fields=  7 bases=1 SymbolVisitor
  methods=  8 fields=  6 bases=0 PathSearch
  methods=  7 fields= 26 bases=0 Parser
  methods=  6 fields=  2 bases=0 AttemptCache
== classes with a base (inheritance) ==
  BodyVisitor -> ['clang::RecursiveASTVisitor']
  SymbolVisitor -> ['clang::RecursiveASTVisitor']
  MatchCallback -> ['clang::ast_matchers::MatchFinder::MatchCallback']
  StoredCompilationDatabase -> ['clang::tooling::CompilationDatabase']
  FactConsumer -> ['clang::ASTConsumer']
  FactAction -> ['clang::ASTFrontendAction']
  IncludeVisitor -> ['clang::PPCallbacks']
```

**What this tells you, and a real gotcha:** typed navigation
(`cb.get(name).methods()`/`.fields()`/`.bases()`) is a per-entity query,
roughly **3 seconds per class** on this codebase, so scanning just 39
classes took nearly two minutes. A first attempt to do this over *all*
1134 project-scoped class names (without excluding lambdas and template
instantiations) was still running after six minutes of CPU time and had to
be killed. There is no aggregate "count members per class" query in the
CXQ grammar; for a whole-codebase architecture sweep, scope to a bounded,
named candidate list (a file glob plus a name filter that excludes
`"(lambda"`/`"<"` names) rather than iterating every symbol.

### 4. Namespaces and the virtual/override inventory

Both queries below are run on the raised-budget `cb` from step 1. Under the
default budget the namespace query comes back `truncated=True` after 37
rows, which is enough to look like a complete answer while quietly missing
namespaces.

```python
q = start(codebase()) | nodes(eq("kind", "namespace")) | select(("name",))
# 42 rows, truncated=False; 26 distinct names:
# ['', 'CLI', 'EmitterStyle', 'ErrorMsg', 'NodeType', 'YAML', 'callgraph', 'catalog',
#  'clang', 'cli', 'commands', 'config', 'conversion', 'detail', 'facts', 'gen_impl',
#  'itlib', 'match', 'platform', 'project_schema', 'recovery', 'std', 'storage',
#  'symbol', 'tooling', 'ui']
q = start(codebase()) | nodes(eq("is_override", True)) | select(("name","file","line"))
# 45 overriding methods, e.g. HandleTranslationUnit, CreateASTConsumer, InclusionDirective,
# BeginSourceFileAction, getCompileCommands (x4), getAllFiles (x4), getAllCompileCommands (x4)
```

**What this tells you:** `is_override`/`is_virtual` are ordinary boolean
symbol fields, queryable directly with `eq()`; no special "virtual" view is
needed. This codebase has 26 distinct namespace names and 45 overriding
declarations in total (project code plus parsed third-party headers
combined, since these are whole-translation-unit counts too, the same
caveat as step 2). Note that the distinct-name list is only trustworthy
because the budget was raised first; the truncated default-budget answer
for the same query is a shorter list, not a flagged error.

## Pitfalls

- **A truncated `count()` returns `None`, not zero, and not an
  under-count.** Always check `.truncated` before trusting a count or
  aggregate on a codebase larger than the default enumeration budget
  (10,000).
- **A truncated `select()` returns a short row list, which is far easier
  to mistake for a complete answer than a `None` count is.** The namespace
  inventory in step 4 loses five names under the default budget. Check
  `.truncated` on row-shaped results too, not just on aggregates.
- **`nodes(eq("kind", "class"))` and similar counts everything a
  translation unit saw**, including standard library, third-party headers,
  template instantiations, and lambda closures, not just your own named
  classes. Filter by a file-path glob, and exclude `"(lambda"`/`"<"` names,
  for a "real class" inventory.
- **Per-entity typed navigation (`cb.get(x).methods()`, `.bases()`, and
  similar) is slow at scale**, on the order of 3 seconds per class
  observed here. There is no aggregate "count members" query; never sweep
  it over thousands of symbols without narrowing the starting set first.

## Where to go next

- [Query Model](../05-python-sdk/03-query-model.md) for the full
  predicate, stage, and shaping vocabulary used above.
- [Views and Catalog](../05-python-sdk/04-views-and-catalog.md) for the
  `symbol` kinds and fields available to query.
- [Relations and Graph Queries](../05-python-sdk/05-relations-and-graph-queries.md)
  for `bases()`, `methods()`, and other typed navigation used in step 3.
- [Tracing Calls and Paths](03-tracing-calls-and-paths.md) to move from
  static structure to call-graph analysis.
