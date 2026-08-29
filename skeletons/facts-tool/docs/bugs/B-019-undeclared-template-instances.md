# B-019 — E2E BDD reproduction

Two independent defects, one document. Every fixture below was run against
`build/facts-tool`; the observed output is recorded verbatim in §5.

Symbols are keyed by **USR**, never by qualified name: every specialization of
`Holder` shares the qualified name `b0xx::Holder`, so a name-keyed assertion
cannot tell a `TSK_Undeclared` specialization from a genuine instantiation.
Verified USR shapes (LLVM 22):

| declaration | USR |
| --- | --- |
| primary template | `c:@N@b0xx@ST>1#T@Holder` |
| `Holder<Widget>` | `c:@N@b0xx@S@Holder>#$@N@b0xx@S@Widget` |
| `Holder<Policy>` | `c:@N@b0xx@S@Holder>#$@N@b0xx@S@Policy` |
| `Holder<int>` | `c:@N@b0xx@S@Holder>#I` |
| `Holder<double>` | `c:@N@b0xx@S@Holder>#d` |
| `Holder<char>` | `c:@N@b0xx@S@Holder>#C` |

Relation kinds used (`src/model/Relation.h:29-62`): `Specializes = 4`,
`Instantiates = 5`, `FieldOf = 8`, `OfType = 20`, `TemplateArgumentType = 23`.

## 1. Fixture A — `tests/fixtures/e2e/undeclared_template_instances.cpp`

```cpp
namespace b0xx {

template <typename T>
struct Holder {
  T value;
};

struct Widget {};
struct Policy {};

// Canary: an ordinary symbol that must survive extraction.
struct Canary {
  int seen;
};

// Named without being required to be complete -> TSK_Undeclared.
Holder<Widget> *pointerOnly = nullptr;
Holder<int> &referenceOnly(Holder<int> &in);
using AliasOnly = Holder<double> *;

struct Owner {
  Holder<char> *field;
};

// Genuine TSK_ImplicitInstantiation, with a record argument so the
// template_argument_type edge is exercised on both sides.
Holder<Policy> instantiated{};

Canary canary{0};

} // namespace b0xx
```

Confirmed with `clang++ -std=c++17 -Xclang -ast-dump`: exactly five
`ClassTemplateSpecializationDecl` nodes — four with no specialization marker
(`TSK_Undeclared`) and one `implicit_instantiation` (`Holder<Policy>`).

## 2. Fixture B — `tests/fixtures/e2e/invalid_usr_declarations.cpp`

```cpp
namespace probe {

struct Bits {
  unsigned named : 3;
  unsigned : 0;   // unnamed bit-field: generateUSRForDecl refuses an identity
};

struct Canary {
  int seen;
};

Bits bits{};
Canary canary{0};

} // namespace probe
```

The production declarations `boost::mpl::aux::le_result1` .. `le_result5` are
**not** reproduced here; their Clang decl kind is still unknown and must be
isolated from the DCS headers before being added. The unnamed bit-field is a
confirmed instance of the same failure class, not a stand-in for those names.

## 3. Feature — `tests/e2e/features/b019_extraction_completeness.feature`

```gherkin
Feature: Extraction completeness for undeclared instances and un-USR-able declarations
  Naming a class template specialization without requiring it to be complete, and
  declaring an entity Clang refuses to give a USR, must both leave extraction
  complete: the unit is committed, provenance is never fabricated, and no
  identity is synthesized.

  Scenario: Undeclared specializations do not abort extraction        # AC 6145
    Given a compile database for the undeclared-template fixture
    When the real extraction command indexes the undeclared-template fixture
    Then the undeclared-template extraction exits successfully without incomplete diagnostics
    And no template_instance relation failure is reported

  Scenario: Undeclared specializations do not discard the unit        # AC 6146
    Given a compile database for the undeclared-template fixture
    When the real extraction command indexes the undeclared-template fixture
    Then the undeclared-template canary and owners are committed

  Scenario: Undeclared specializations record no provenance           # AC 6147
    Given a compile database for the undeclared-template fixture
    When the real extraction command indexes the undeclared-template fixture
    Then each undeclared specialization carries no specializes and no instantiates relation
    And the undeclared record-argument specialization still carries its template argument type edge

  Scenario: Genuine instantiations keep their provenance              # AC 6148
    Given a compile database for the undeclared-template fixture
    When the real extraction command indexes the undeclared-template fixture
    Then the implicitly instantiated specialization points to the primary template
    And the implicitly instantiated specialization carries its template argument type edge

  Scenario: Un-USR-able declarations are skipped, not failed          # AC 6149
    Given a compile database for the invalid-usr fixture
    When the real extraction command indexes the invalid-usr fixture
    Then the invalid-usr extraction exits successfully without incomplete diagnostics
    And no symbol row exists for the un-USR-able declaration
    And no relation references the un-USR-able declaration
    And no identity was synthesized for the un-USR-able declaration

  Scenario: Un-USR-able declarations are traced and siblings survive  # AC 6150
    Given a compile database for the invalid-usr fixture
    When the real extraction command indexes the invalid-usr fixture at verbosity 3
    Then the trace records the skipped declaration with reason invalid USR
    And every named sibling declaration in the same record is committed

  Scenario Outline: Extraction is deterministic and referentially intact  # AC 6152
    Given a compile database for the <fixture> fixture
    When the real extraction command indexes the <fixture> fixture twice
    Then both runs produce identical symbol identities
    And the output database passes PRAGMA foreign_key_check

    Examples:
      | fixture             |
      | undeclared-template |
      | invalid-usr         |
```

## 4. Steps — `tests/e2e/steps/b019_extraction_completeness_steps.py`

```python
from __future__ import annotations

import json
import subprocess
from pathlib import Path

from pytest_bdd import given, parsers, scenarios, then, when
from support.database import query, require
from support.scenario import FactsToolContext

SPECIALIZES = 4
INSTANTIATES = 5
FIELD_OF = 8
TEMPLATE_ARGUMENT_TYPE = 23

PRIMARY_USR = "c:@N@b0xx@ST>1#T@Holder"
UNDECLARED_USRS = {
    "Holder<Widget>": "c:@N@b0xx@S@Holder>#$@N@b0xx@S@Widget",
    "Holder<int>": "c:@N@b0xx@S@Holder>#I",
    "Holder<double>": "c:@N@b0xx@S@Holder>#d",
    "Holder<char>": "c:@N@b0xx@S@Holder>#C",
}
INSTANTIATED_USR = "c:@N@b0xx@S@Holder>#$@N@b0xx@S@Policy"

FIXTURES = {
    "undeclared-template": ("undeclared_template_instances.cpp", "b019a"),
    "invalid-usr": ("invalid_usr_declarations.cpp", "b019b"),
}


def run(command: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, capture_output=True, text=True, check=False)


def symbol_id(context: FactsToolContext, usr: str) -> int:
    rows = query(
        context.facts_database_path,
        "SELECT id FROM symbol WHERE usr=?",
        (usr,),
    )
    require(len(rows) == 1, f"expected exactly one symbol for usr {usr!r}, got {rows}")
    return rows[0][0]


@given(
    parsers.parse("a compile database for the {fixture} fixture"),
    target_fixture="b019_fixture",
)
def given_b019_compile_database(context: FactsToolContext, fixture: str) -> Path:
    context.prepare()
    name, slug = FIXTURES[fixture]
    source = (context.fixture_root / name).resolve(strict=True)
    context.facts_database = context.run_root_path / f"{slug}-facts.sqlite"
    context.files_database = context.run_root_path / f"{slug}-project.sqlite"
    (context.run_root_path / "compile_commands.json").write_text(
        json.dumps(
            [
                {
                    "directory": str(context.fixture_root),
                    "file": str(source),
                    "arguments": [
                        str(context.compiler),
                        "-std=c++17",
                        "-c",
                        str(source),
                    ],
                }
            ],
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )
    return source


def extract_once(context: FactsToolContext, source: Path, verbosity: int = 1) -> None:
    imported = run(
        [
            str(context.facts_tool),
            "import",
            "--conf",
            str(context.files_database_path),
            "--compilation-database",
            str(context.run_root_path),
            str(source),
        ]
    )
    require(
        imported.returncode == 0,
        f"expected import exit code 0, got {imported.returncode}:\n"
        + imported.stdout
        + imported.stderr,
    )
    completed = run(
        [
            str(context.facts_tool),
            "extract",
            "--output",
            str(context.facts_database_path),
            "--conf",
            str(context.files_database_path),
            "--verbose",
            str(verbosity),
            str(source),
        ]
    )
    context.last_returncode = completed.returncode
    context.last_output = completed.stdout + completed.stderr


@when(parsers.parse("the real extraction command indexes the {fixture} fixture"))
def when_extract_indexes(
    context: FactsToolContext, b019_fixture: Path, fixture: str
) -> None:
    extract_once(context, b019_fixture)


@when(
    parsers.parse(
        "the real extraction command indexes the {fixture} fixture at verbosity 3"
    )
)
def when_extract_indexes_verbose(
    context: FactsToolContext, b019_fixture: Path, fixture: str
) -> None:
    extract_once(context, b019_fixture, verbosity=3)


@when(parsers.parse("the real extraction command indexes the {fixture} fixture twice"))
def when_extract_indexes_twice(
    context: FactsToolContext, b019_fixture: Path, fixture: str
) -> None:
    extract_once(context, b019_fixture)
    context.first_identities = query(
        context.facts_database_path,
        "SELECT usr,qualified_name FROM symbol ORDER BY usr,qualified_name",
    )
    context.facts_database_path.unlink()
    extract_once(context, b019_fixture)


# --- AC 6145 ---------------------------------------------------------------


@then(
    parsers.parse(
        "the {fixture} extraction exits successfully without incomplete diagnostics"
    )
)
def then_extraction_succeeds(context: FactsToolContext, fixture: str) -> None:
    require(
        context.last_returncode == 0,
        f"expected extract exit code 0, got {context.last_returncode}:\n"
        + context.last_output,
    )
    require(
        "indexing incomplete" not in context.last_output,
        f"unexpected incomplete diagnostic:\n{context.last_output}",
    )


@then("no template_instance relation failure is reported")
def then_no_template_instance_failure(context: FactsToolContext) -> None:
    require(
        "relation=template_instance" not in context.last_output,
        f"template_instance relation failure reported:\n{context.last_output}",
    )
    require(
        "rollback output transaction" not in context.last_output,
        f"extraction rolled back the translation unit:\n{context.last_output}",
    )


# --- AC 6146 ---------------------------------------------------------------


@then("the undeclared-template canary and owners are committed")
def then_unit_is_committed(context: FactsToolContext) -> None:
    symbols = {
        row[0]
        for row in query(
            context.facts_database_path,
            "SELECT qualified_name FROM symbol WHERE qualified_name LIKE 'b0xx::%'",
        )
    }
    expected = {
        "b0xx::Canary",
        "b0xx::canary",
        "b0xx::Widget",
        "b0xx::Policy",
        "b0xx::Holder",
        "b0xx::Owner",
        "b0xx::Owner::field",
        "b0xx::pointerOnly",
        "b0xx::referenceOnly",
        "b0xx::AliasOnly",
        "b0xx::instantiated",
    }
    require(
        expected <= symbols,
        f"translation unit facts were discarded, missing: {expected - symbols}",
    )
    # All six Holder declarations must exist as distinct rows.
    holders = query(
        context.facts_database_path,
        "SELECT usr FROM symbol WHERE qualified_name='b0xx::Holder' ORDER BY usr",
    )
    expected_usrs = sorted(
        [PRIMARY_USR, INSTANTIATED_USR, *UNDECLARED_USRS.values()]
    )
    require(
        [usr for (usr,) in holders] == expected_usrs,
        f"unexpected Holder specialization set: {holders}",
    )


# --- AC 6147 ---------------------------------------------------------------


@then("each undeclared specialization carries no specializes and no instantiates relation")
def then_undeclared_have_no_provenance(context: FactsToolContext) -> None:
    offenders = {}
    for label, usr in UNDECLARED_USRS.items():
        sid = symbol_id(context, usr)
        rows = query(
            context.facts_database_path,
            "SELECT kind,destination_id FROM relation "
            "WHERE source_id=? AND kind IN (?,?)",
            (sid, SPECIALIZES, INSTANTIATES),
        )
        if rows:
            offenders[label] = rows
    require(
        offenders == {},
        f"TSK_Undeclared specializations fabricated provenance relations: {offenders}",
    )


@then(
    "the undeclared record-argument specialization still carries its template argument type edge"
)
def then_undeclared_keeps_argument_edge(context: FactsToolContext) -> None:
    sid = symbol_id(context, UNDECLARED_USRS["Holder<Widget>"])
    widget = symbol_id(context, "c:@N@b0xx@S@Widget")
    rows = query(
        context.facts_database_path,
        "SELECT destination_id,position FROM relation WHERE source_id=? AND kind=?",
        (sid, TEMPLATE_ARGUMENT_TYPE),
    )
    require(
        rows == [(widget, 0)],
        f"Holder<Widget> lost its template_argument_type edge: {rows}",
    )


# --- AC 6148 ---------------------------------------------------------------


@then("the implicitly instantiated specialization points to the primary template")
def then_instantiation_keeps_provenance(context: FactsToolContext) -> None:
    sid = symbol_id(context, INSTANTIATED_USR)
    primary = symbol_id(context, PRIMARY_USR)
    rows = query(
        context.facts_database_path,
        "SELECT kind,destination_id FROM relation "
        "WHERE source_id=? AND kind IN (?,?)",
        (sid, SPECIALIZES, INSTANTIATES),
    )
    require(
        rows == [(INSTANTIATES, primary)],
        f"Holder<Policy> lost or mis-typed its provenance relation: {rows}",
    )


@then("the implicitly instantiated specialization carries its template argument type edge")
def then_instantiation_keeps_argument_edge(context: FactsToolContext) -> None:
    sid = symbol_id(context, INSTANTIATED_USR)
    policy = symbol_id(context, "c:@N@b0xx@S@Policy")
    rows = query(
        context.facts_database_path,
        "SELECT destination_id,position FROM relation WHERE source_id=? AND kind=?",
        (sid, TEMPLATE_ARGUMENT_TYPE),
    )
    require(
        rows == [(policy, 0)],
        f"Holder<Policy> lost its template_argument_type edge: {rows}",
    )


# --- AC 6149 ---------------------------------------------------------------


@then("no symbol row exists for the un-USR-able declaration")
def then_no_symbol_row_for_skipped(context: FactsToolContext) -> None:
    rows = query(
        context.facts_database_path,
        "SELECT id,usr,qualified_name FROM symbol "
        "WHERE qualified_name LIKE 'probe::Bits::%'",
    )
    require(
        [(usr, name) for _, usr, name in rows]
        == [("c:@N@probe@S@Bits@FI@named", "probe::Bits::named")],
        f"the un-USR-able bit-field was persisted, or a sibling was lost: {rows}",
    )


@then("no relation references the un-USR-able declaration")
def then_no_relation_references_skipped(context: FactsToolContext) -> None:
    record = symbol_id(context, "c:@N@probe@S@Bits")
    named = symbol_id(context, "c:@N@probe@S@Bits@FI@named")
    rows = query(
        context.facts_database_path,
        "SELECT source_id,destination_id,kind FROM relation "
        "WHERE destination_id=? AND kind=?",
        (record, FIELD_OF),
    )
    require(
        rows == [(named, record, FIELD_OF)],
        f"a field_of edge references a declaration that has no identity: {rows}",
    )


@then("no identity was synthesized for the un-USR-able declaration")
def then_no_synthesized_identity(context: FactsToolContext) -> None:
    # 1. Nothing that names an unnamed entity was persisted.
    unnamed = query(
        context.facts_database_path,
        "SELECT usr,qualified_name FROM symbol "
        "WHERE qualified_name LIKE '%(anonymous)%' "
        "OR qualified_name LIKE '%(unnamed)%'",
    )
    require(unnamed == [], f"an unnamed entity was persisted: {unnamed}")

    # 2. Every persisted identity is a Clang USR. Any fallback the extractor
    #    could invent (file+offset, a synthetic prefix) would not carry the
    #    'c:' prefix that clang::index::generateUSRForDecl always emits.
    fabricated = query(
        context.facts_database_path,
        "SELECT usr,qualified_name FROM symbol WHERE usr NOT LIKE 'c:%'",
    )
    require(
        fabricated == [],
        f"an identity outside Clang's USR namespace was synthesized: {fabricated}",
    )

    # 3. The record's membership is exact, not merely a superset: the skipped
    #    bit-field contributed no row under any identity.
    members = query(
        context.facts_database_path,
        "SELECT usr FROM symbol WHERE usr LIKE 'c:@N@probe@S@Bits@%' ORDER BY usr",
    )
    require(
        members == [("c:@N@probe@S@Bits@FI@named",)],
        f"unexpected member set under probe::Bits: {members}",
    )


# --- AC 6150 ---------------------------------------------------------------


@then("the trace records the skipped declaration with reason invalid USR")
def then_trace_records_skip(context: FactsToolContext) -> None:
    expected = (
        "facts-tool: trace: node extraction kind='Field' "
        "name='probe::Bits::(anonymous)' result=filtered reason='invalid USR'"
    )
    require(
        expected in context.last_output,
        "verbosity-3 trace did not record the skip as filtered with reason "
        f"'invalid USR'; expected:\n{expected}\ngot:\n{context.last_output}",
    )


@then("every named sibling declaration in the same record is committed")
def then_named_siblings_survive(context: FactsToolContext) -> None:
    symbols = {
        row[0]
        for row in query(
            context.facts_database_path,
            "SELECT qualified_name FROM symbol WHERE qualified_name LIKE 'probe::%'",
        )
    }
    expected = {
        "probe::Bits",
        "probe::Bits::named",
        "probe::Canary",
        "probe::Canary::seen",
        "probe::bits",
        "probe::canary",
    }
    require(
        expected <= symbols,
        f"declarations around the skipped entity were lost: {expected - symbols}",
    )


# --- AC 6152 ---------------------------------------------------------------


@then("both runs produce identical symbol identities")
def then_identities_are_stable(context: FactsToolContext) -> None:
    second = query(
        context.facts_database_path,
        "SELECT usr,qualified_name FROM symbol ORDER BY usr,qualified_name",
    )
    require(
        context.first_identities == second,
        "symbol identities changed between two extractions of the same unit",
    )


@then("the output database passes PRAGMA foreign_key_check")
def then_foreign_keys_are_intact(context: FactsToolContext) -> None:
    violations = query(context.facts_database_path, "PRAGMA foreign_key_check")
    require(violations == [], f"foreign-key violations: {violations}")


scenarios("../features/b019_extraction_completeness.feature")
```

Register the module in `tests/e2e/conftest.py:8-35`. `context.first_identities`
is a new scratch attribute on `FactsToolContext`; add it as an optional field
alongside `last_output`.

## 5. Observed failure today

### Fixture A

```
$ facts-tool import -c conf.db --extra-arg=-std=c++17 undeclared_template_instances.cpp
$ facts-tool extract -c conf.db -o facts.db -v 2
facts-tool: extract: begin output transaction
facts-tool: extract: Clang parse and AST extraction
facts-tool: indexing incomplete: cannot persist relation=template_instance \
    source='b0xx::Holder' target='b0xx::Holder' usr='<unavailable>': Invalid argument
facts-tool: extract: rollback output transaction
$ echo $?; sqlite3 facts.db "select count(*) from symbol;"
1
0
```

Scenarios for AC 6145–6148 all fail: exit 1, the `indexing incomplete` line is
present, and `symbol` is empty after the rollback, so every USR lookup raises.

### Fixture B

```
$ facts-tool extract -c conf.db -o facts.db -v 3
facts-tool: trace: node extraction kind='Field' name='probe::Bits::named' result=success
facts-tool: trace: node extraction kind='Field' name='probe::Bits::(anonymous)' result=failure reason='invalid USR'
facts-tool: indexing incomplete: cannot extract symbol 'probe::Bits::(anonymous)': invalid USR
$ echo $?; sqlite3 facts.db "select count(*) from symbol;"
1
0
```

Scenarios for AC 6149, 6150 and 6152 fail: exit 1, the trace says
`result=failure` where the policy requires `result=filtered`, and the unit is
rolled back.

## 6. Fixture that does NOT reproduce (rejected)

```cpp
namespace repro {
template <typename T> struct identity { using type = T; };
template <typename T> struct wrapper { using value_type = typename identity<T>::type; };
template <typename T> struct instance : wrapper<T> {};
instance<int> value;
}
```

Verified against `build/facts-tool`: exit 0, 13 symbols, no diagnostics. Every
specialization here is a genuine `TSK_ImplicitInstantiation`, which already
works. Do not use it.

## 7. Attribution note

`relation=template_instance` with `usr='<unavailable>'` is emitted only by
`TemplateSpecialization.cpp:264-269`, the `relationKind()` failure branch. The
`store.addRelations()` failure site at `TemplateSpecialization.cpp:314-319`
prints `instantiates`/`specializes` and a real USR, so it is not the source of
these diagnostics. `src/storage/Relation.cpp:10-27` inserts with
`ON CONFLICT ... DO NOTHING`, so duplicate or self edges are not the failure
either.
