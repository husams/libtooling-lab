from __future__ import annotations

import sqlite3

from pytest_bdd import given, when, then, parsers
from support.database import query, require
from support.scenario import FactsToolContext


# Source anchors disambiguate overloads and lambda closures without assuming
# compiler-specific generated names or USRs. Each anchor owns one callable.
CASES = {
    "free_function": ("int free_function()", "int"),
    "free_void": ("void free_void()", "void"),
    "free_value": ("Widget free_value()", "return_types::Widget"),
    "free_pointer": ("Widget* free_pointer(", "return_types::Widget *"),
    "free_reference": ("const Widget& free_reference(", "const return_types::Widget &"),
    "trailing_return": ("auto trailing_return()", "return_types::Widget"),
    "method": ("const Widget& method()", "const return_types::Widget &"),
    "static_method": ("static int static_method()", "int"),
    "object_value": ("Widget operator()()", "return_types::Widget"),
    "object_int": ("int operator()(int", "int"),
    "explicit_lambda": ("auto explicit_lambda =", "return_types::Widget *"),
    "deduced_lambda": ("auto deduced_lambda =", "int"),
}


def callable_row(context, selector):
    anchor, _ = CASES[selector]
    lines = (context.fixture_root / "return_types.cpp").read_text().splitlines()
    locations = [i + 1 for i, line in enumerate(lines) if anchor in line]
    require(len(locations) == 1, f"ambiguous fixture anchor: {anchor}")
    rows = query(context.facts_database_path,
                 "SELECT id,usr,qualified_name FROM symbol WHERE node=1 AND line=?",
                 (locations[0],))
    if "lambda" in selector:
        rows = [row for row in rows if "operator()" in row[2]]
    require(len(rows) == 1, f"missing or ambiguous callable {selector}: {rows}")
    return rows[0]


def return_facts(context, selector, spelling):
    identity, usr, name = callable_row(context, selector)
    edges = query(context.facts_database_path,
                  "SELECT destination_id,position FROM relation "
                  "WHERE source_id=? AND kind=21", (identity,))
    # Assert edges first so the pre-fix regression fails on the actual omission.
    require(len(edges) == 1 and edges[0][1] == 0,
            f"missing or duplicate persisted ReturnType edge for {selector}: {edges}")
    destination = edges[0][0]
    if spelling in ("int", "void"):
        require(0 < destination < 2**32,
                f"{selector} must use a predefined FileId-0 type: {destination}")
        require(query(context.facts_database_path,
                      "SELECT qualified_name FROM symbol WHERE id=?", (destination,)) == [(spelling,)],
                f"wrong predefined return type for {selector}")
        if spelling == "int":
            parameter_types = query(context.facts_database_path,
                                    "SELECT p.type FROM parameter p JOIN symbol s ON s.id=p.symbol_id "
                                    "WHERE s.qualified_name='return_types::FunctionObject::operator()'")
            require((destination,) in parameter_types, "int return target differs from parameter.type")
    else:
        target = query(context.facts_database_path,
                       "SELECT qualified_name FROM symbol WHERE id=?", (destination,))
        require(target == [("return_types::Widget",)], f"wrong return target: {target}")
    types = query(context.facts_database_path,
                  "SELECT canonical_type FROM callable_return_type WHERE symbol_id=?",
                  (identity,))
    require(types == [(spelling,)], f"incorrect canonical return type for {selector}: {types}")
    return identity, usr, name, destination, spelling


def inventory(context):
    facts = [return_facts(context, key, value[1]) for key, value in CASES.items()]
    ids = [row[0] for row in facts]
    require(len(set(ids)) == len(CASES), "fixture selectors reused a callable identity")
    placeholders = ",".join("?" for _ in ids)
    edges = query(context.facts_database_path,
                  f"SELECT source_id,destination_id,position FROM relation "
                  f"WHERE kind=21 AND source_id IN ({placeholders}) ORDER BY source_id",
                  tuple(ids))
    require(edges == sorted((r[0], r[3], 0) for r in facts),
            f"unexpected scoped ReturnType inventory: {edges}")
    primitives = {r[4]: r[3] for r in facts if r[4] in ("int", "void")}
    require(len(set(primitives.values())) == 2, "void and int must be distinct")
    return facts


@given("the isolated C++17 callable return-type fixture")
def prepare_callable_fixture(context: FactsToolContext):
    context.sources = (context.fixture_root / "return_types.cpp",)
    context.prepare()
    path = context.run_root_path / "compile_commands.json"
    path.write_text(path.read_text().replace("-std=c++23", "-std=c++17"))


@when("I import and extract the callable fixture through the real CLI")
def extract_callables(context: FactsToolContext):
    context.extract()
    require("indexing incomplete" not in context.last_output, context.last_output)


@then(parsers.parse('callable "{selector}" has exactly one return edge and canonical type "{type}"'))
def verify_callable(context: FactsToolContext, selector, type):
    context.return_fact = return_facts(context, selector, type)


@then("the callable return type is displayed by the supported symbol query")
def query_callable(context: FactsToolContext):
    identity, _, name, _, spelling = context.return_fact
    result = context._run([str(context.facts_tool), "symbol", "show", name,
                           "--facts", str(context.facts_database_path),
                           "--conf", str(context.files_database_path)])
    require(result.returncode == 0, context.last_output)
    # Overload output must contain the matching identity and return spelling
    # in the same declaration block, rather than matching another overload.
    packed = f"{identity >> 32}:{identity & (2**32 - 1)}"
    blocks = result.stdout.split("\n\n")
    require(any(f"identity   {packed}\n" in block and
                f" -> {spelling}\n" in block for block in blocks), result.stdout)


@then("every selected callable has exactly its expected return-type facts")
def verify_inventory(context: FactsToolContext):
    context.return_inventory = inventory(context)


@when("I extract the callable fixture again into the same database")
def extract_again(context: FactsToolContext):
    context.run_tool()
    require("indexing incomplete" not in context.last_output, context.last_output)


@then("callable identities and return-type facts are unchanged without duplicates")
def verify_repeat(context: FactsToolContext):
    require(inventory(context) == context.return_inventory, "return facts changed on re-extraction")


def unrelated_facts(context):
    return [query(context.facts_database_path, sql) for sql in (
        "SELECT * FROM symbol ORDER BY id",
        "SELECT * FROM parameter ORDER BY symbol_id,position",
        "SELECT * FROM relation WHERE kind<>21 ORDER BY source_id,destination_id,kind,position",
    )]


@given("the callable facts database has the previous schema without return types")
def previous_schema(context: FactsToolContext):
    context.before_upgrade = unrelated_facts(context)
    with sqlite3.connect(context.facts_database_path) as db:
        db.execute("DELETE FROM relation WHERE kind=21")
        db.execute("DROP TABLE callable_return_type")
        db.execute("PRAGMA user_version=8")


@then("the upgrade preserves existing identities and unrelated facts")
def verify_upgrade(context: FactsToolContext):
    require(unrelated_facts(context) == context.before_upgrade, "upgrade changed unrelated facts")
    require(query(context.facts_database_path, "PRAGMA user_version") == [(9,)],
            "return-type schema migration did not advance to version 9")
    context.run_tool()
    require(unrelated_facts(context) == context.before_upgrade, "second migration changed facts")
    require(inventory(context) == context.return_inventory, "second migration changed return facts")


@then("legacy callable symbols remain queryable without rewriting the database")
def query_legacy(context: FactsToolContext):
    before = context.facts_database_path.read_bytes()
    result = context._run([str(context.facts_tool), "symbol", "show",
                           "return_types::free_function", "--facts",
                           str(context.facts_database_path)])
    require(result.returncode == 0 and "return_types::free_function()" in result.stdout,
            context.last_output)
    require(" -> " not in result.stdout, "legacy query invented a missing return type")
    require(context.facts_database_path.read_bytes() == before,
            "read-only symbol query rewrote the old facts schema")


@then("the declaration catalog excludes predefined return targets without deleting facts")
def declaration_catalog(context: FactsToolContext):
    before = context.facts_database_path.read_bytes()
    builtins = query(context.facts_database_path,
                     "SELECT qualified_name FROM symbol WHERE (id >> 32)=0")
    require(set(builtins) == {("int",), ("void",)},
            f"the fixture must retain its predefined return targets: {builtins}")
    expected = query(context.facts_database_path,
                     "SELECT qualified_name FROM symbol WHERE (id >> 32)<>0")
    # List and browser share loadSymbols; compare the entire declaration set
    # so excluding bookkeeping rows cannot silently discard real declarations.
    result = context._run([str(context.facts_tool), "symbol", "list", "--facts",
                           str(context.facts_database_path)])
    require(result.returncode == 0, context.last_output)
    listed = [line.split(maxsplit=1)[1] for line in result.stdout.splitlines()[1:]]
    require(sorted(listed) == sorted(name for (name,) in expected),
            f"declaration catalog contains wrong symbols: {result.stdout}")
    require(context.facts_database_path.read_bytes() == before,
            "listing declarations changed stored return targets")
    require(inventory(context) == context.return_inventory,
            "listing declarations changed return facts")
