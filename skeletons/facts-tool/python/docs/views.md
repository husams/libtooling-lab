# View and field catalog

Facts views are `symbol`, `parameter`, `template_parameter`,
`template_argument`, `edge`, `site`, `definition`, `enumeration`, `enumerator`,
`initializer`, and `return_type`. Project views are `repository`, `clone`,
`component`, `directory`, and `file`.

Symbols expose identity, USR, qualified and short names, raw `kind_id`, mapped
`kind`, node kind, declaration FileId/path/line/column/offset, access,
properties, return spelling, and every stored qualifier: definition, implicit,
static, virtual, const, volatile, inline, pure, override, linkage/external,
variadic, deleted/defaulted/explicit/final/abstract/polymorphic, constexpr kind,
noexcept, and reference qualifier.

`kind_id` is the stored LLVM 22 `clang::index::SymbolKind` integer, mapped
without renumbering. The complete numeric table is in [Symbol kinds](symbol-kinds.md).

Parameters expose owner, source order, name, packed type identity, location and
region, pointer/reference/forwarding-reference/const/pack flags, and distinct
`has_default`, expression, evaluated kind, and evaluated value fields.

Public `template_parameter` means a declared slot and reads physical
`template_argument`: name, type, order, parameter-pack, non-type, and
template-template flags. Public `template_argument` means a supplied value and
reads physical `template_parameter`: value/type, order, kind, pack index, and
pointer/reference/const/pack flags.

Definitions expose their own FileId/path, offset, and size. Enumerations expose
underlying type, scoped, and fixed flags; enumerators expose value and written
initializer. Initializers and parameter defaults retain written and evaluated
forms. Return spelling is also available as the `return_type` side-table view.

Project repositories expose kind, remote, active clone, and semantic universe;
clones expose repository/path/label; components expose path/version/ownership;
directories expose component-relative paths. Files expose resolved path, hash,
mtime, driver, compile options, working directory, indexed state/time, and
override state.

All identifiers are allowlisted per view. Unknown fields fail with `E_FIELD`
even for an empty result, so a typo cannot masquerade as a valid negative.
