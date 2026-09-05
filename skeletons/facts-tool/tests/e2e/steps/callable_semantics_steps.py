from pytest_bdd import then
from support.database import query


@then("special member overloads retain their semantic properties")
def special_members(context):
    rows = query(context.facts_database_path,
                 "SELECT usr,is_defaulted,is_deleted,is_explicit,is_noexcept,"
                 "is_const,is_volatile,ref_qualifier FROM symbol "
                 "WHERE qualified_name='qualifiers::Special::Special'")
    assert len(rows) == 3, rows
    assert len({r[0] for r in rows}) == 3
    assert {r[1:4] for r in rows} == {(1, 0, 0), (0, 1, 0), (0, 0, 1)}
    assert all(r[5:] == (0, 0, "none") for r in rows), rows
    assert all(r[4] == 1 for r in rows if r[1] or r[3]), rows
    destructor = query(context.facts_database_path,
                       "SELECT is_defaulted,is_noexcept FROM symbol "
                       "WHERE qualified_name='qualifiers::Special::~Special'")
    assert destructor == [(1, 1)], destructor


@then("callable template instances retain conditional noexcept and explicit")
def conditional(context):
    rows = query(context.facts_database_path,
                 "SELECT qualified_name,usr,is_noexcept,is_explicit,is_const "
                 "FROM symbol WHERE qualified_name LIKE 'qualifiers::%conditional%' "
                 "OR qualified_name LIKE 'qualifiers::Conditional%'")
    for prefix in ("qualifiers::conditional", "qualifiers::Specs::conditional"):
        group = [r for r in rows if r[0].startswith(prefix)]
        assert len(group) == 3, group
        assert len({r[1] for r in group}) == 3
        assert sorted(r[2] for r in group) == [0, 0, 1], group
        assert all(r[4] == int("Specs" in prefix) for r in group), group
        for r in group:
            if "#Vb1" in r[1]:
                assert r[2] == 1, r
            if "#Vb0" in r[1]:
                assert r[2] == 0, r
    for suffix in ("::Conditional", "::operator bool"):
        group = [r for r in rows if "@F@" in r[1] and (r[0].endswith(suffix) or
                 (suffix == "::Conditional" and r[0].endswith("::Conditional<N>")))]
        assert len(group) == 3, group
        assert sorted((r[2], r[3]) for r in group) == [(0, 0), (0, 0), (1, 1)], group
        for r in group:
            expected = (1, 1) if "#Vb1" in r[1] else (0, 0)
            assert r[2:4] == expected, r


@then("callable lambda operators retain const and noexcept")
def lambdas(context):
    rows = query(context.facts_database_path,
                 "SELECT line,is_const,is_noexcept,usr FROM symbol WHERE "
                 "qualified_name LIKE 'qualifiers::instantiate%operator()%'")
    assert rows, "lambda call operators were not extracted"
    source = context.fixture_root / "qualifiers" / "specifiers.hpp"
    declarations = {name: next(i for i, line in enumerate(source.read_text().splitlines(), 1)
                               if f"auto {name} =" in line)
                    for name in ("fixed", "mutableLambda", "generic", "mutableGeneric")}
    for name, line in declarations.items():
        group = [r for r in rows if r[0] == line]
        assert len(group) == (2 if "eneric" in name else 1), (name, rows)
        expected = (0, 0) if name.startswith("mutable") else (1, 1)
        assert all(r[1:3] == expected for r in group), (name, group)
