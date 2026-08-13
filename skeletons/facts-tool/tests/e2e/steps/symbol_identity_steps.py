from __future__ import annotations

from collections import defaultdict

from support.bdd import step
from support.database import query, require, scalar, symbol_snapshot
from support.scenario import FactsToolContext


@step("SymbolIds are packed from stable, sequential per-file indices")
def then_symbol_ids_are_per_file_and_sequential(context: FactsToolContext) -> None:
    by_file: dict[int, list[tuple[int, int]]] = defaultdict(list)
    for packed, file_id, file_index in query(
        context.facts_database_path,
        "SELECT id,file_id,file_index FROM symbol ORDER BY file_id,file_index",
    ):
        by_file[file_id].append((file_index, packed))

    require(by_file, "expected captured symbols")
    for file_id, indexed_symbols in by_file.items():
        indices = [index for index, _ in indexed_symbols]
        require(
            indices == list(range(len(indices))),
            f"non-sequential indices for FileId {file_id}: {indices}",
        )
        require(
            all(packed == (file_id << 32) | index for index, packed in indexed_symbols),
            f"invalid packed SymbolId for FileId {file_id}",
        )
        next_index = scalar(
            context.facts_database_path,
            "SELECT next_index FROM symbol_allocator WHERE file_id=?",
            (file_id,),
        )
        require(
            next_index == len(indices),
            f"allocator drift for FileId {file_id}",
        )


@step("repeated declarations and translation units reuse each USR's SymbolId")
def then_repeated_declarations_reuse_usr_identity(
    context: FactsToolContext,
) -> None:
    expected_repeated = {
        "e2e",
        "e2e::Deferred",
        "e2e::Payload",
        "e2e::Policy",
        "e2e::Widget",
        "e2e::headerHelper",
        "e2e::transform",
    }
    symbols = {
        qualified_name: (symbol_id, file_id, file_index, node, usr)
        for symbol_id, file_id, file_index, node, usr, qualified_name in symbol_snapshot(
            context.facts_database_path
        )
    }
    require(
        expected_repeated <= symbols.keys(),
        f"missing repeated declarations: {expected_repeated - symbols.keys()}",
    )
    for name in expected_repeated:
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

    duplicates = query(
        context.facts_database_path,
        "SELECT usr,COUNT(*) FROM symbol WHERE usr<>'' GROUP BY usr HAVING COUNT(*)<>1",
    )
    require(not duplicates, f"duplicate logical symbols: {duplicates}")
