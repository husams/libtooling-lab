#include "commands/catalog/Commands.h"
#include "storage/Sqlite.h"
#include "storage/SqliteDatabase.h"
#include "storage/catalog/Database.h"
#include <array>
#include <format>
#include <iostream>

namespace facts::commands {
namespace {

struct SymbolFact {
  SymbolId id;
  std::array<std::string, 32> details;
};

catalog::Result<std::vector<SymbolFact>> symbols(storage::Database &database,
                                                 const std::string &name) {
  return catalog::query(
      database,
      "SELECT id,node,kind,sub_kind,lang,properties,usr,qualified_name,line,"
      "col,offset,access,is_definition,is_implicit,is_static,is_virtual,"
      "is_const,is_inline,is_pure,ref_qualifier,is_override,"
      "has_internal_linkage,is_external,is_variadic,is_deleted,is_defaulted,"
      "is_explicit,is_final,is_abstract,is_polymorphic,has_extern_storage,"
      "constant_evaluation,is_noexcept FROM symbol "
      "WHERE (?='' OR qualified_name=?) ORDER BY id",
      [](const storage::Row &row) {
        SymbolFact value;
        value.id = row.get<SymbolId>(0);
        for (int column = 1; column <= 32; ++column)
          value.details[static_cast<std::size_t>(column - 1)] =
              column == 6 || column == 7 || column == 11 || column == 19 ||
                      column == 31
                  ? row.string(column)
                  : std::to_string(row.integer(column));
        return value;
      },
      name, name);
}

std::string displaySymbols(const std::vector<SymbolFact> &values) {
  if (values.empty())
    return "No symbols\n";
  std::string output = "ID\tFILE\tINDEX\tQUALIFIED NAME\tUSR\n";
  for (const auto &value : values)
    output +=
        std::format("{}\t{}\t{}\t{}\t{}\n", value.id.packed(), value.id.file,
                    value.id.index, value.details[6], value.details[5]);
  return output;
}

std::string displaySymbol(const SymbolFact &value) {
  static constexpr std::array labels = {"NODE",
                                        "KIND",
                                        "SUB KIND",
                                        "LANG",
                                        "PROPERTIES",
                                        "USR",
                                        "QUALIFIED NAME",
                                        "LINE",
                                        "COLUMN",
                                        "OFFSET",
                                        "ACCESS",
                                        "DEFINITION",
                                        "IMPLICIT",
                                        "STATIC",
                                        "VIRTUAL",
                                        "CONST",
                                        "INLINE",
                                        "PURE",
                                        "REF QUALIFIER",
                                        "OVERRIDE",
                                        "INTERNAL LINKAGE",
                                        "EXTERNAL",
                                        "VARIADIC",
                                        "DELETED",
                                        "DEFAULTED",
                                        "EXPLICIT",
                                        "FINAL",
                                        "ABSTRACT",
                                        "POLYMORPHIC",
                                        "EXTERN STORAGE",
                                        "CONSTANT EVALUATION",
                                        "NOEXCEPT"};
  std::string output =
      std::format("ID: {}\nFILE ID: {}\nSYMBOL INDEX: {}\n", value.id.packed(),
                  value.id.file, value.id.index);
  for (std::size_t index = 0; index < labels.size(); ++index)
    output += std::format("{}: {}\n", labels[index], value.details[index]);
  return output;
}

catalog::Result<storage::Database> openFacts(const std::string &path) {
  return storage::Database::open(path, storage::Database::readOnly)
      .transform_error([](const auto &error) {
        return "cannot open facts database: " + error.message();
      });
}

catalog::Result<std::string> operate(storage::Database &database,
                                     const cli::SymbolOptions &options) {
  return symbols(database, options.action == cli::SymbolOptions::Action::show
                               ? options.qualifiedName
                               : std::string{})
      .and_then([&](const auto &values) -> catalog::Result<std::string> {
        if (options.action == cli::SymbolOptions::Action::list)
          return displaySymbols(values);
        if (values.empty())
          return std::unexpected("symbol '" + options.qualifiedName +
                                 "' not found");
        std::string output;
        for (const auto &value : values)
          output += displaySymbol(value);
        return output;
      });
}

} // namespace

catalog::Result<int> runSymbol(const cli::SymbolOptions &options) {
  return openFacts(options.facts)
      .and_then([&](auto database) { return operate(database, options); })
      .transform([](const auto &output) {
        std::cout << output;
        return 0;
      });
}

} // namespace facts::commands
