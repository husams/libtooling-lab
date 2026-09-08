# Quick start: a ten-minute tour

This chapter takes a tiny, real C++ project from nothing through import,
extraction, a symbol lookup, a call-graph run, and one query from Python.
Every command and every line of output below was actually run; only long
output has been trimmed.

You need a built `facts-tool` binary and (for the last step) the Python SDK
installed - see [03-installation](03-installation.md) if you haven't done
that yet. Throughout, `$FT` means the path to your `facts-tool` checkout,
and `$PROJ` is the tiny demo project directory you'll create in step 1.

## 1. A tiny project

The demo project is three small files: an abstract `Shape` base class with
two implementations, a free function that calls a shape's virtual `area()`,
and a `main()` that builds a vector of shapes, wraps a lambda in a
`std::function`, and prints a total.

`$PROJ/include/shapes.hpp`:

```cpp
#pragma once

#include <string>

namespace shapes {

class Shape {
public:
  virtual ~Shape() = default;
  virtual double area() const = 0;
  virtual std::string name() const { return "Shape"; }
};

class Circle : public Shape {
public:
  explicit Circle(double radius) : radius_(radius) {}
  double area() const override;
  std::string name() const override { return "Circle"; }

private:
  double radius_;
};

class Square : public Shape {
public:
  explicit Square(double side) : side_(side) {}
  double area() const override;
  std::string name() const override { return "Square"; }

private:
  double side_;
};

template <typename T> T doubled(T value) { return value + value; }

double describe(const Shape &shape);

} // namespace shapes
```

`$PROJ/src/shapes.cpp`:

```cpp
#include "shapes.hpp"

namespace shapes {

double Circle::area() const { return 3.14159265358979 * radius_ * radius_; }

double Square::area() const { return side_ * side_; }

double describe(const Shape &shape) { return shape.area(); }

} // namespace shapes
```

`$PROJ/src/main.cpp`:

```cpp
#include "shapes.hpp"

#include <functional>
#include <iostream>
#include <memory>
#include <vector>

namespace {

double totalArea(const std::vector<std::unique_ptr<shapes::Shape>> &items) {
  double total = 0.0;
  for (const auto &item : items) {
    total += shapes::describe(*item);
  }
  return total;
}

} // namespace

int main() {
  std::vector<std::unique_ptr<shapes::Shape>> items;
  items.push_back(std::make_unique<shapes::Circle>(2.0));
  items.push_back(std::make_unique<shapes::Square>(3.0));

  std::function<double(double)> scale = [](double value) {
    return shapes::doubled(value);
  };

  const double total = totalArea(items);
  std::cout << "total area: " << scale(total) << "\n";
  return 0;
}
```

## 2. Generate a `compile_commands.json`

`facts-tool import` reads a standard `compile_commands.json`. For a small,
hand-written project without a build system, you can generate one directly
instead of running a build:

```python
import json

proj = "/absolute/path/to/PROJ"
compiler = "/opt/homebrew/opt/llvm/bin/clang++"  # your Homebrew LLVM clang++
sources = ["src/shapes.cpp", "src/main.cpp"]

entries = [
    {
        "directory": proj,
        "file": f"{proj}/{src}",
        "arguments": [compiler, "-std=c++17", "-Iinclude", "-c", f"{proj}/{src}"],
    }
    for src in sources
]

with open(f"{proj}/compile_commands.json", "w") as f:
    json.dump(entries, f, indent=2)
```

You don't need `-isysroot` or `-resource-dir` entries: `facts-tool`
resolves the SDK path and the compiler's resource directory itself at
runtime, injecting them automatically whenever a stored command lacks them.

## 3. Register the project and import compile commands

Register the project as a repository, then import its compile commands.
Run these from a working directory of your choice; `demo.db` is the project
database this creates:

```console
$ facts-tool repo add demo "$PROJ" -c demo.db
Repository registered

$ facts-tool import -c demo.db -p "$PROJ" -v 1
facts-tool: import: starting
facts-tool: import: parse components
facts-tool: import: load compilation database
facts-tool: import: open project database
facts-tool: import: read file registry
facts-tool: import: store compile commands
facts-tool: import: register files
[1/2] Processing file .../proj/src/shapes.cpp.
[2/2] Processing file .../proj/src/main.cpp.
facts-tool: import: complete
Imported 2 compile command(s)
Registered 820 file(s)
```

820 registered files for a 2-source, 2-header project is expected, not a
bug: `import` discovers and registers every transitively included header
too - every libc++ and SDK header pulled in by `<vector>`, `<memory>`,
`<functional>`, and `<iostream>`. Don't be alarmed by a `dir list`/`file
list` full of Homebrew LLVM and Xcode SDK paths later; that's the normal
shape of a real C++ project's include closure.

Do **not** also run `component add` for this project before importing -
`import -p DIR` already creates its own component automatically, and
combining the two breaks catalog inspection commands later. See
[02-projects-and-configuration/01-repositories-and-projects](../02-projects-and-configuration/01-repositories-and-projects.md)
for the full explanation and the recommended workflow.

## 4. Extract facts

```console
$ facts-tool extract -c demo.db -o demo-facts.db -v 1 "$PROJ/src/shapes.cpp" "$PROJ/src/main.cpp"
facts-tool: extract: starting
facts-tool: extract: validate database paths
facts-tool: extract: load compilation database
facts-tool: extract: validate stored commands
facts-tool: extract: extract facts
facts-tool: extract: open project database
facts-tool: extract: validate registry completeness
facts-tool: extract: select sources
facts-tool: extract: resolve registered sources
[1/2] Processing file .../proj/src/shapes.cpp.
[2/2] Processing file .../proj/src/main.cpp.
facts-tool: extract: configure Clang tool
facts-tool: extract: open output database
facts-tool: extract: begin output transaction
facts-tool: extract: Clang parse and AST extraction
[1/2] Processing file .../proj/src/shapes.cpp.
facts-tool: coverage.unsupported_semantics kind=implicit-cleanup site=.../c++/v1/__chrono/duration.h:340:8
... (12 more coverage.unsupported_semantics lines for shapes.cpp) ...
[2/2] Processing file .../proj/src/main.cpp.
... (18 more, one of them at proj/src/main.cpp:25:41) ...
facts-tool: extract: commit output transaction
facts-tool: 83 symbol(s) recorded from 24 file(s)
facts-tool: extract: complete
```

The two sources are listed twice because the run resolves the registered
source set before handing it to the Clang tool, and all of this output goes
to standard error rather than standard output.

The `facts-tool: coverage.unsupported_semantics kind=implicit-cleanup
site=...` lines cover implicit destructor calls the extractor couldn't
attribute a precise call-site column to (mostly deep in libc++ internals,
occasionally a project lambda's own implicit cleanup). This is routine noise
on any real C++ translation unit that includes the standard library. It does
not affect the exit code or the recorded symbol count, and `-v 0` does not
suppress it; only the `facts-tool: extract:` stage lines respond to
verbosity. See
[03-extracting-facts/01-extract](../03-extracting-facts/01-extract.md#verbosity-levels)
for what each level actually changes.

## 5. Look up a symbol

```console
$ facts-tool symbol show 'shapes::Circle::area' -f demo-facts.db -c demo.db
shapes::Circle::area() const -> double
  identity   820:7
  kind       instance-method
  type       Function
  language   C++
  access     public
  source     shapes.hpp:17:10
             .../proj/include/shapes.hpp
  usr        c:@N@shapes@S@Circle@F@area#1
  properties none
  flags      definition, virtual, const, override
```

The `identity` line (`820:7`) is `<file_id>:<index>` - the packed identity
`facts-tool` and the SDK use to refer to this exact symbol.

## 6. Run a call graph

`analyse call-graph` never prints a graph to your terminal - it persists an
append-only run in the facts database and prints exactly one completion
line:

```console
$ facts-tool analyse call-graph -c demo.db -f demo-facts.db --function main -v 1
facts-tool: call-graph: starting
facts-tool: roots selected
facts-tool: graph traversal
facts-tool: call-graph: complete
facts-tool: call graph run 1 complete
```

`analyse call-graph` never writes a graph to your terminal beyond that one
line - the actual edges live in the facts database's `callgraph_run*`
tables and must be read back, either with the Python SDK (next step) or
with raw SQL for a quick look:

```console
$ sqlite3 demo-facts.db "SELECT s1.qualified_name, s2.qualified_name, e.kind, e.depth
    FROM callgraph_run_edge e
    JOIN symbol s1 ON s1.id = e.source_id
    JOIN symbol s2 ON s2.id = e.destination_id
    WHERE e.run_id = 1 ORDER BY e.depth;"
main|(anonymous namespace)::totalArea|1|1
main|std::function<type-parameter-0-0 (type-parameter-0-1...)>::function<_Rp (_ArgTypes...)>|1|1
... 15 more depth-1 STL constructor/destructor/operator() edges ...
(anonymous namespace)::totalArea|std::__1::operator!=|1|2
... 5 more depth-2 STL iterator edges ...
(anonymous namespace)::totalArea|shapes::describe|1|2
shapes::describe|shapes::Shape::area|1|3
shapes::describe|shapes::Circle::area|18|3
shapes::describe|shapes::Square::area|18|3
```

That is 27 edges in total: 17 at depth 1, 7 at depth 2, and 3 at depth 3.

Kind `1` is a static `Calls` edge; kind `18` is `DispatchCalls` - the
traversal correctly resolved the virtual `Shape::area()` call inside
`describe(const Shape&)` to both `Circle::area` and `Square::area` as
conservative dispatch targets. See
[04-call-graphs/01-overview](../04-call-graphs/01-overview.md) for the full
model.

## 7. One query from the Python SDK

Raw SQL works, but it means re-deriving the schema every time and re-doing
that yourself if the schema changes. The documented, schema-checked way to
read a run back is through `CodeBase.callgraphs`:

```console
$ python -c "
from facts_tool import open_codebase
with open_codebase(facts_db='demo-facts.db', project_db='demo.db') as cb:
    run = cb.callgraphs.get(1)
    print(run.status, run.roots.total, run.edges.total)
    for e in run.edges:
        print(e.source.qualified_name, '->', e.target.qualified_name, e.semantic_kind, e.depth)
"
complete 1 27
main -> (anonymous namespace)::totalArea Calls 1
main -> std::function<type-parameter-0-0 (type-parameter-0-1...)>::function<_Rp (_ArgTypes...)> Calls 1
...
(anonymous namespace)::totalArea -> shapes::describe Calls 2
shapes::describe -> shapes::Shape::area Calls 3
shapes::describe -> shapes::Circle::area DispatchCalls 3
shapes::describe -> shapes::Square::area DispatchCalls 3
```

This is the same run you just created from the CLI, read back through the
SDK's schema-checked, paginated reader instead of raw SQL - the pattern to
reach for whenever a query needs to survive schema changes or run inside an
agent. See
[05-python-sdk/06-persisted-callgraph-runs](../05-python-sdk/06-persisted-callgraph-runs.md)
for the full reader API, including paging through large runs.

## Where to go next

- [02-projects-and-configuration](../02-projects-and-configuration/01-repositories-and-projects.md)
  covers the project catalog (`repo`, `component`, `dir`, `file`) and
  configuration precedence in depth.
- [03-extracting-facts](../03-extracting-facts/01-extract.md) covers
  `extract` options, incremental extraction, and what exactly gets
  recorded.
- [04-call-graphs](../04-call-graphs/01-overview.md) covers roots, budgets,
  recovery, and every `analyse call-graph` option.
- [05-python-sdk](../05-python-sdk/01-getting-started.md) covers the full
  query language, typed graph API, and error model.
- [06-workflows](../06-workflows/08-using-facts-tool-from-an-ai-agent.md)
  shows how to wire this all up for an AI coding agent.
