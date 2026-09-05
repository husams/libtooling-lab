# LLVM 22 symbol kinds

facts-tool persists `clang::index::SymbolKind` directly. The Python `kind`
field maps the integer without shifting or inventing categories.

| ID | `kind` | ID | `kind` |
|---:|---|---:|---|
| 0 | `unknown` | 16 | `enum_constant` |
| 1 | `module` | 17 | `instance_method` |
| 2 | `namespace` | 18 | `class_method` |
| 3 | `namespace_alias` | 19 | `static_method` |
| 4 | `macro` | 20 | `instance_property` |
| 5 | `include_directive` | 21 | `class_property` |
| 6 | `enum` | 22 | `static_property` |
| 7 | `struct` | 23 | `constructor` |
| 8 | `class` | 24 | `destructor` |
| 9 | `protocol` | 25 | `conversion_function` |
| 10 | `extension` | 26 | `parameter` |
| 11 | `union` | 27 | `using` |
| 12 | `type_alias` | 28 | `template_type_parm` |
| 13 | `function` | 29 | `template_template_parm` |
| 14 | `variable` | 30 | `non_type_template_parm` |
| 15 | `field` | 31 | `concept` |

Unknown future integer values are returned as `kind_N` so stored data remains
visible while the catalog reports the package's supported LLVM mapping.
