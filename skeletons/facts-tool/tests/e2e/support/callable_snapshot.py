import sqlite3


def snapshot(path):
    with sqlite3.connect(path) as db:
        values = {name: db.execute(f"SELECT * FROM {name} ORDER BY 1,2").fetchall()
                  for name in ("symbol", "parameter_default", "relation",
                               "definition", "template_parameter", "template_argument")}
        # Parameter spelling locations can belong to either redeclaration;
        # compare the parameter contract, not the last visited source span.
        values["parameter"] = db.execute(
            "SELECT symbol_id,position,name,type,is_pointer,is_lvalue_reference,"
            "is_rvalue_reference,is_forwarding_reference,is_const,is_pack,has_default "
            "FROM parameter ORDER BY symbol_id,position").fetchall()
        return values


def semantics(path):
    with sqlite3.connect(path) as db:
        return db.execute(
            "SELECT usr,is_const,is_volatile,ref_qualifier,is_noexcept,"
            "constant_evaluation,is_explicit,is_variadic,is_inline,is_static,"
            "is_deleted,is_defaulted,is_virtual,is_pure,is_override,is_final "
            "FROM symbol ORDER BY usr").fetchall()
