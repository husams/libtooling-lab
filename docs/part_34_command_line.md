# Part 34 — Command-Line APIs

[← Part 33 — AST Matchers](part_33_ast_matchers.md) | [Part 35 — Advanced Tooling →](part_35_advanced.md)

Your tools already *have* a command line — `--help` on either skeleton prints
pages of options you never wrote. This part explains where those come from,
how to add your own flags, and how the file-vs-flags plumbing
(`CommonOptionsParser`, compilation databases) really works.

## 34.1 — llvm::cl in ten minutes

LLVM's command-line library is declarative: an option is a **global
variable**, registration happens at static-init time, and parsing fills the
globals in.

```cpp
#include "llvm/Support/CommandLine.h"

static llvm::cl::opt<bool> Verbose(
    "verbose",
    llvm::cl::desc("Print every declaration visited"),
    llvm::cl::init(false));

static llvm::cl::opt<std::string> Output(
    "o", llvm::cl::desc("Write report to file"), llvm::cl::value_desc("path"));

static llvm::cl::list<std::string> Checks(
    "check", llvm::cl::desc("Enable a named check (repeatable)"));
```

After parsing, the globals convert implicitly: `if (Verbose) …`,
`for (auto &C : Checks) …`. The main templates are `cl::opt<T>` (single
value; `bool`, `int`, `std::string`, or any enum via `cl::values(…)`),
`cl::list<T>` (repeatable), and `cl::alias` (a second spelling). Useful
modifiers: `cl::init` (default), `cl::Required`, `cl::Positional`,
`cl::value_desc` (the placeholder in `--help`).

Because registration is global for the whole process, and your process links
**all of LLVM**, option names share one namespace with LLVM's own internals.

⚠️ Pick a name LLVM already registered — say, `cl::opt<std::string>
Filter("filter", …)` — and the tool *aborts at startup*:

```
CommandLine Error: Option 'filter' registered more than once!
LLVM ERROR: inconsistency in registered CommandLine options
```

Nothing tells you at compile time. Prefix or hyphenate your names
(`--name-filter`, `--lab-verbose`) and you'll never collide.

## 34.2 — Custom options for your tool

Both skeletons already contain the two lines that make custom options
first-class citizens of a Clang tool:

```cpp
static llvm::cl::OptionCategory ToolCategory("visitor-tool options");
…
auto Options = CommonOptionsParser::create(argc, argv, ToolCategory);
```

`CommonOptionsParser` owns the tooling command-line contract:

```
visitor-tool [tool options] <source files…> [-p <build dir>] [-- <compile flags…>]
```

It registers the standard tooling options (`-p`, `--extra-arg`, …), parses
positionals as source paths, splits everything after `--` off as compile
flags — and, crucially, **hides every registered option that isn't in your
category** from `--help`. That's why the skeletons' help output is readable
instead of listing 300 LLVM developer flags. To enroll your options, tag
them:

```cpp
static llvm::cl::opt<bool> Verbose("verbose", llvm::cl::desc("…"),
                                   llvm::cl::cat(ToolCategory));
```

(Third-party code sometimes needs the same trick manually —
`llvm::cl::HideUnrelatedOptions(ToolCategory)` — but `create()` does it for
you.)

## 34.3 — Hands-on: --verbose and --name-filter

Give visitor-tool a real interface:

1. `--verbose` (`cl::opt<bool>`): when off, print one summary line per class;
   when on, also print methods and fields.
2. `--name-filter=<substring>` (`cl::opt<std::string>`): only report
   declarations whose qualified name contains the substring
   (`StringRef(Name).contains(Filter)`).

Both must carry `cl::cat(ToolCategory)` — verify with `--help` that they
show up under "visitor-tool options" and that the LLVM noise stays hidden.

```bash
./build/visitor-tool sample/sample.cpp --name-filter=Circle --verbose -- -std=c++17
```

Design note: the globals are visible from your visitor because *everything*
can see globals — that's the pragmatic, slightly ugly llvm::cl trade-off,
and every Clang tool from clang-tidy down accepts it. Keep option access at
the edges (decide in the callback whether to report), don't thread the
globals through your analysis logic.

**Quiz.** Where do `--name-filter=Circle` and `-std=c++17` each get parsed,
and why must `--name-filter` come *before* the `--`?

> [!success]- Answer
> `--name-filter` is consumed by llvm::cl in your process.
> `-std=c++17` is never parsed by llvm::cl at all — everything after `--`
> goes verbatim into the compilation database and is parsed by the Clang
> *front-end* each time a file is processed. After the `--` you're not
> talking to your tool anymore; you're talking to the compiler inside it.

## 34.4 — Compilation databases

`CommonOptionsParser` decides where compile flags come from, in priority
order:

1. **`--` on the command line** → `FixedCompilationDatabase`: same flags for
   every file. What the lab has used throughout.
2. **`-p <dir>`** → loads `<dir>/compile_commands.json`.
3. **Neither** → search for `compile_commands.json` upward from each source
   file's directory.

`compile_commands.json` is the real-project answer: one entry per TU with
its *exact* flags, generated by CMake with
`-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` (the skeletons already set it — look in
`build/`):

```json
{ "directory": "…/skeletons/visitor-tool/build",
  "command": "…/clang++ -std=gnu++17 … -c …/main.cpp",
  "file": "…/main.cpp" }
```

Try the tool on **its own source**, no `--` this time:

```bash
./build/visitor-tool -p build main.cpp | head -20
```

Your visitor walks the tool's own AST — LibTooling analyzing LibTooling —
with each file parsed under its true build flags. (The skeleton's
ArgumentsAdjuster applies to database entries too, which is why this needs
no extra plumbing. The generic knob, if you ever need to add a flag to
every entry by hand, is the standard tooling option `--extra-arg=<flag>`.) This is the whole point of
the database: at real-project scale ("run the check over all 2,000 TUs"),
per-file flags differ, and `--` can't express that.

The programmatic layer is worth knowing for Part 35:
`CompilationDatabase::loadFromDirectory(…)`,
`FixedCompilationDatabase(Directory, Args)`, and
`Options->getCompilations()` — the object the `ClangTool` constructor takes.
