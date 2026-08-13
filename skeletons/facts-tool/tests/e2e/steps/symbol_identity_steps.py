from __future__ import annotations

from collections import defaultdict

from support.bdd import Table, table_records, then
from support.database import query, require, scalar, symbol_snapshot
from support.scenario import FactsToolContext


def symbols_by_file(context: FactsToolContext) -> dict[int, list[tuple[int, int]]]:
    result: dict[int, list[tuple[int, int]]] = defaultdict(list)
    for packed, file_id, file_index in query(
        context.facts_database_path,
        "SELECT id,file_id,file_index FROM symbol ORDER BY file_id,file_index",
    ):
        result[file_id].append((file_index, packed))
    require(result, "expected captured symbols")
    return result


@then("every file has sequential zero-based symbol indices")
def then_file_symbol_indices_are_sequential(context: FactsToolContext) -> None:
    for file_id, indexed_symbols in symbols_by_file(context).items():
        indices = [index for index, _ in indexed_symbols]
        require(
            indices == list(range(len(indices))),
            f"non-sequential indices for FileId {file_id}: {indices}",
        )


@then("every SymbolId packs its FileId and file index")
def then_symbol_ids_are_packed(context: FactsToolContext) -> None:
    for file_id, indexed_symbols in symbols_by_file(context).items():
        require(
            all(packed == (file_id << 32) | index for index, packed in indexed_symbols),
            f"invalid packed SymbolId for FileId {file_id}",
        )


@then("every symbol allocator points to the next file index")
def then_allocators_point_to_next_index(context: FactsToolContext) -> None:
    for file_id, indexed_symbols in symbols_by_file(context).items():
        next_index = scalar(
            context.facts_database_path,
            "SELECT next_index FROM symbol_allocator WHERE file_id=?",
            (file_id,),
        )
        require(
            next_index == len(indexed_symbols),
            f"allocator drift for FileId {file_id}",
        )


@then("these repeated declarations have one nonempty USR each")
def then_repeated_declarations_have_one_usr(
    context: FactsToolContext, table: Table
) -> None:
    expected = {row["qualified_name"] for row in table_records(table)}
    symbols = {
        qualified_name: (symbol_id, file_id, file_index, node, usr)
        for symbol_id, file_id, file_index, node, usr, qualified_name in symbol_snapshot(
            context.facts_database_path
        )
    }
    require(
        expected <= symbols.keys(),
        f"missing repeated declarations: {expected - symbols.keys()}",
    )
    for name in expected:
        usr = symbols[name][4]
        require(usr, f"expected a USR for {name}")
        require(
            scalar(
                context.facts_database_path,
                "SELECT COUNT(*) FROM symbol WHERE usr=?",
                (usr,),
            )
            == 1,
            f"USR was duplicated for {name}",
        )


@then("no nonempty USR identifies more than one stored symbol")
def then_no_usr_is_duplicated(context: FactsToolContext) -> None:
    duplicates = query(
        context.facts_database_path,
        "SELECT usr,COUNT(*) FROM symbol WHERE usr<>'' GROUP BY usr HAVING COUNT(*)<>1",
    )
    require(not duplicates, f"duplicate logical symbols: {duplicates}")
