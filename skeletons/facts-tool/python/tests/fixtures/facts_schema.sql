CREATE TABLE symbol (
 id INTEGER PRIMARY KEY, node INTEGER, kind INTEGER, sub_kind INTEGER, lang INTEGER,
 properties INTEGER, usr TEXT, qualified_name TEXT, line INTEGER, col INTEGER,
 offset INTEGER, access TEXT, is_definition INTEGER, is_implicit INTEGER,
 is_static INTEGER, is_virtual INTEGER, is_const INTEGER, is_inline INTEGER,
 is_pure INTEGER, ref_qualifier TEXT, is_override INTEGER,
 has_internal_linkage INTEGER, is_external INTEGER, is_variadic INTEGER,
 is_deleted INTEGER, is_defaulted INTEGER, is_explicit INTEGER, is_final INTEGER,
 is_abstract INTEGER, is_polymorphic INTEGER, has_extern_storage INTEGER,
 constant_evaluation TEXT, is_noexcept INTEGER, is_volatile INTEGER
);
CREATE TABLE callable_return_type (symbol_id INTEGER PRIMARY KEY, canonical_type TEXT);
CREATE TABLE definition (symbol_id INTEGER PRIMARY KEY, file_id INTEGER,
 offset INTEGER, size INTEGER);
CREATE TABLE enumeration (symbol_id INTEGER PRIMARY KEY, underlying_type INTEGER,
 is_scoped INTEGER, has_fixed_underlying_type INTEGER);
CREATE TABLE enumerator (symbol_id INTEGER PRIMARY KEY, value TEXT,
 initializer_expression TEXT);
CREATE TABLE variable_initializer (symbol_id INTEGER PRIMARY KEY, expression TEXT,
 evaluated_kind TEXT, evaluated_value TEXT);
CREATE TABLE parameter (
 symbol_id INTEGER, position INTEGER, name TEXT, type INTEGER, line INTEGER,
 col INTEGER, offset INTEGER, region_offset INTEGER, region_size INTEGER,
 is_pointer INTEGER, is_lvalue_reference INTEGER, is_rvalue_reference INTEGER,
 is_forwarding_reference INTEGER, is_const INTEGER, is_pack INTEGER,
 has_default INTEGER, PRIMARY KEY(symbol_id, position)
);
CREATE TABLE parameter_default (symbol_id INTEGER, position INTEGER, expression TEXT,
 evaluated_kind TEXT, evaluated_value TEXT, PRIMARY KEY(symbol_id, position));
CREATE TABLE template_argument (symbol_id INTEGER, position INTEGER, name TEXT,
 type_id INTEGER, is_parameter_pack INTEGER, is_non_type INTEGER,
 is_template_template INTEGER, PRIMARY KEY(symbol_id, position));
CREATE TABLE template_parameter (
 symbol_id INTEGER, position INTEGER, value TEXT, type_id INTEGER,
 is_pointer INTEGER, is_lvalue_reference INTEGER, is_rvalue_reference INTEGER,
 is_forwarding_reference INTEGER, is_const INTEGER, is_pack INTEGER,
 kind INTEGER, pack_index INTEGER, PRIMARY KEY(symbol_id, position)
);
CREATE TABLE relation (
 source_id INTEGER, destination_id INTEGER, kind INTEGER, position INTEGER,
 access TEXT, is_virtual_base INTEGER, is_implicit INTEGER, is_lexical INTEGER,
 count INTEGER, PRIMARY KEY(source_id,destination_id,kind,position)
);
CREATE TABLE relation_site (
 source_id INTEGER, destination_id INTEGER, kind INTEGER, position INTEGER,
 file_id INTEGER, line INTEGER, col INTEGER, offset INTEGER,
 receiver_type_id INTEGER, certainty INTEGER,
 PRIMARY KEY(source_id,destination_id,kind,position,file_id,offset)
);
CREATE TABLE include_dependency (src_file_id INTEGER, dst_file_id INTEGER,
 PRIMARY KEY(src_file_id,dst_file_id));
PRAGMA user_version=10;
