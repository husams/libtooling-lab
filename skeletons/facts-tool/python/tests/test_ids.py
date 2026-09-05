from facts_tool import SymbolId


def test_symbol_identity_round_trips_unsigned_64_bit() -> None:
    identity = SymbolId(0xFFFFFFFF, 0xFFFFFFFF)
    assert identity.packed == 0xFFFFFFFFFFFFFFFF
    assert identity.sqlite == -1
    assert SymbolId.unpack(identity.sqlite) == identity
    assert identity.to_dict()["packed"] == "18446744073709551615"
