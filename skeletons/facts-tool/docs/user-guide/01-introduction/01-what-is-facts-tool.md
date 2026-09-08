# What is facts-tool?

`facts-tool` is a standalone Clang LibTooling executable that walks the
Clang AST of a C++ codebase and records what it finds as structured facts in
a SQLite database. Its own build description puts it plainly: it is a
"skeleton for a fact-extraction tool over the Clang AST." It never compiles,
links, or runs the code it analyzes. It only reads source through Clang's
parser and writes out symbols, relations, and locations.

Around that native extractor sits a small family of companion tools:

- **`facts-tool`** (C++, this guide's core subject) - extracts facts from a
  compilation database or a fixed list of sources, manages a project
  catalog, and runs call-graph traversals.
- **`facts-tool-query`** (Python package, import name `facts_tool`) - a
  separate, read-only SDK that opens the SQLite databases `facts-tool`
  produces and lets you query them with a declarative, composable query
  language instead of hand-written SQL.
- **`facts-tool-batch`** - a process-fanout wrapper that runs one
  `facts-tool` invocation per source file with bounded parallelism, for
  large codebases.
- An **agent skill** (`facts-tool-code-reasoning`) that teaches an AI coding
  agent to answer C++ questions from these facts instead of re-reading or
  re-scanning source.

This chapter explains what each piece is for and how they relate. The rest
of the guide is organized so you can go from "I have a C++ project" to
"I can query its structure and call graph" in the order the chapters appear:
see [02-concepts-and-glossary](02-concepts-and-glossary.md) for vocabulary,
[03-installation](03-installation.md) to build and install everything, and
[04-quick-start](04-quick-start.md) for a ten-minute walkthrough.

## How extraction works, at a glance

Each invocation of `facts-tool` is one `ClangTool` run built from one
`FrontendActionFactory`. For every translation unit (TU) Clang parses, the
tool gets one `FrontendAction`, one `ASTConsumer`, and the fully-built
`ASTContext` once Clang finishes parsing. An extractor then walks that
context and hands facts to a `FactStore`, which persists them.

Traversal itself is deliberately simple: a `Traversal` builds one
`SymbolVisitor`, calls `TraverseDecl()` exactly once on the translation
unit's root declaration, and then works through a queue of scheduled
function and lambda bodies - each body walked exactly once by a
`BodyVisitor`. There is no general-purpose AST event log kept around for
later replay; each TU is visited, its facts are written, and the visitor
state for that TU is done.

Because it only ever reads the AST that Clang itself built, `facts-tool`
inherits Clang's own accuracy for name resolution, template instantiation,
and overload resolution - it does not re-implement any C++ semantics of its
own.

## The two databases

Every extraction and every query works against a **pair** of SQLite
databases, always used together:

- A **facts database** holding symbols, fact side tables (parameters,
  templates, enumerations, and so on), relations between symbols, the
  precise source locations ("sites") where those relations occur, and
  include-dependency facts.
- A **project database** (also called the project/configuration database,
  or catalog) holding repositories, the checkout clones registered under
  them, project components, indexed directories, registered files, and the
  compile configuration used to extract each file.

The native CLI writes to both. The Python SDK only ever opens them
read-only. Chapter [02-projects-and-configuration/01-repositories-and-projects](../02-projects-and-configuration/01-repositories-and-projects.md)
covers the project database's catalog in detail, and
[07-reference/02-storage-schema.md](../07-reference/02-storage-schema.md)
documents both schemas table by table.

## High-level architecture

`facts-tool`'s C++ sources are organized into a handful of libraries, from
its `CMakeLists.txt` library targets:

| Library | Responsibility |
|---|---|
| `facts-model` | Header-only value types shared by extraction and storage |
| `facts-storage` | SQLite persistence: the facts schema, the project/catalog schema, call-graph run history, and the matched-symbol index |
| `facts-tooling` | Reading and writing stored compilation databases, and project import |
| `facts-config` | YAML configuration defaults and CLI precedence resolution |

Those four are the ones worth knowing by name. `CMakeLists.txt` defines
several more targets in the same style, each wrapping one source directory:
`facts-ast` (the Clang visitors and extractors that walk the AST),
`facts-cli` (command-line option parsing and dispatch), `facts-commands`
(command orchestration, including the catalog commands and the
`analyse call-graph` implementation), plus `facts-platform`, `facts-symbols`,
`facts-symbol-ui`, and interface targets for Clang and SQLite.

SQLite itself is vendored as a statically-linked amalgamation by default, so
behavior is identical across macOS and Linux regardless of the system's
installed SQLite version - notably because RHEL 9's system SQLite (3.34)
lacks the `RETURNING` clause that the storage layer relies on. Configuring
with `-DFACTS_SYSTEM_SQLITE=ON` links the system library instead, and then
requires it to be 3.35 or newer.

## How facts-tool relates to other tools in this workspace

Two relationships come up often enough to be worth stating explicitly, so
you don't confuse `facts-tool` with either of them:

**It is not the Python `libclang-lab`.** That is a separate, unrelated lab
built on the C `libclang` API from Python - different API, different
language, different goals. No further technical relationship exists between
the two beyond both being C++ analysis exercises in the same workspace.

**It is the smaller sibling of a project called cidx ("cpp-indexer").**
cidx is a larger, separate indexing system that explicitly adopted
`facts-tool`'s *traversal-ownership* pattern - scheduling function and
lambda bodies into a queue instead of recursing into them while walking the
tree, with one canonical-owner queue per TU. cidx deliberately did **not**
adopt `facts-tool`'s direct-to-SQLite write path, because cidx needs
concurrent isolated workers, deferred cross-TU identity resolution, and a
batched writer that a single-writer tool like `facts-tool` doesn't need.
Concretely: `facts-tool` is the smaller reference implementation of the
traversal idea; cidx is the larger system that borrowed the idea but not
the storage design. The Python SDK's own query language is, separately, a
deliberate copy of cidx's declarative query-plan design - "this package
copies the declarative design, not indexing APIs or cidx storage
assumptions." The SDK never invokes Clang, `libclang`, cidx, or the native
`facts-tool` executable itself; it only reads the SQLite files.

## What's next

- [02-concepts-and-glossary](02-concepts-and-glossary.md) defines the terms
  used throughout this guide (facts database, project database, pairing,
  freshness, coverage, USR, relation, site, run, and more).
- [03-installation](03-installation.md) builds the native tool and installs
  the Python SDK.
- [04-quick-start](04-quick-start.md) is a hands-on ten-minute tour that
  extracts facts from a small project and queries them both from the CLI
  and from Python.
