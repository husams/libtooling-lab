from pytest_bdd import given, when, then, parsers
from support.database import query
from support.table import table_records


@given("the callable qualifier project")
def callable_project(context):
    context.sources = tuple(context.fixture_root / "qualifiers" / name
                            for name in ("one.cpp", "two.cpp"))
    context.prepare()


@when("the qualifier project is extracted")
def extract_callables(context):
    context.extract()


@then(parsers.parse('callable "{name}" has exactly one overload with '
                    '{constant:d}, {volatile:d}, {ref}'))
def cv_overload(context, name, constant, volatile, ref):
    rows = query(context.facts_database_path,
                 "SELECT usr FROM symbol WHERE qualified_name=? "
                 "AND is_const=? AND is_volatile=? AND ref_qualifier=?",
                 (name, constant, volatile, ref))
    assert len(rows) == 1, (name, constant, volatile, ref, rows)
    all_rows = query(context.facts_database_path,
                     "SELECT usr FROM symbol WHERE qualified_name=?", (name,))
    assert len(set(all_rows)) == (4 if name.endswith("plain") else 8)


@then("callable properties are")
def callable_properties(context, datatable):
    for row in table_records(datatable):
        name = row.pop("qualified_name")
        columns = tuple(row)
        assert all(c.replace("_", "").isalnum() for c in columns)
        actual = query(context.facts_database_path,
                       f"SELECT {','.join(columns)} FROM symbol "
                       "WHERE qualified_name=?", (name,))
        expected = tuple(row[c] for c in columns)
        assert actual and all(tuple(str(v) for v in r) == expected
                              for r in actual), (name, expected, actual)
