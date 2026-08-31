#include "commands/catalog/Commands.h"
#include "storage/Storage.h"
#include "storage/Sqlite.h"
#include "storage/SqliteDatabase.h"
#include "storage/catalog/Database.h"
#include "storage/catalog/File.h"
#include "clang/Index/IndexSymbol.h"
#include <algorithm>
#include <array>
#include <filesystem>
#include <format>
#include <iostream>
#include <optional>
#include <string_view>
#include <vector>

namespace facts::commands {
namespace {

struct SymbolFact {
  SymbolId id;
  std::array<std::string, 32> details;
  std::int64_t line;
  std::string type;
  std::string kind;
  std::string source;
};

struct SourceFile {
  FileId id;
  std::string name;
  std::filesystem::path path;
};

constexpr std::string_view symbolPropertyName(
    clang::index::SymbolProperty property) noexcept {
  switch (property) {
  case clang::index::SymbolProperty::Generic:
    return "Generic";
  case clang::index::SymbolProperty::TemplatePartialSpecialization:
    return "TemplatePartialSpecialization";
  case clang::index::SymbolProperty::TemplateSpecialization:
    return "TemplateSpecialization";
  case clang::index::SymbolProperty::UnitTest:
    return "UnitTest";
  case clang::index::SymbolProperty::IBAnnotated:
    return "IBAnnotated";
  case clang::index::SymbolProperty::IBOutletCollection:
    return "IBOutletCollection";
  case clang::index::SymbolProperty::GKInspectable:
    return "GKInspectable";
  case clang::index::SymbolProperty::Local:
    return "Local";
  case clang::index::SymbolProperty::ProtocolInterface:
    return "ProtocolInterface";
  }
  return "Unknown";
}

std::string symbolProperties(std::int64_t properties) {
  std::string result;
  clang::index::applyForEachSymbolProperty(
      static_cast<clang::index::SymbolPropertySet>(properties),
      [&result](const auto property) {
        if (!result.empty())
          result += ", ";
        result += symbolPropertyName(property);
      });
  return result.empty() ? "none" : result;
}

bool isBooleanColumn(int column) {
  switch (column) {
  case 12:
  case 13:
  case 14:
  case 15:
  case 16:
  case 17:
  case 18:
  case 20:
  case 21:
  case 22:
  case 23:
  case 24:
  case 25:
  case 26:
  case 27:
  case 28:
  case 29:
  case 30:
  case 32:
    return true;
  default:
    return false;
  }
}

std::string symbolDetail(const storage::Row &row, int column) {
  if (column == 6 || column == 7 || column == 11 || column == 19 ||
      column == 31)
    return row.string(column);
  if (isBooleanColumn(column))
    return row.integer(column) ? "yes" : "no";
  return std::to_string(row.integer(column));
}

std::string sourceLocation(const std::vector<SourceFile> &files, SymbolId id,
                           std::int64_t line) {
  const auto file = std::ranges::find(files, id.file, &SourceFile::id);
  if (file == files.end())
    return std::format("<file {}>@<unknown>:{}", id.file, line);
  return std::format("{}@{}:{}", file->name, file->path.string(), line);
}

catalog::Result<std::vector<SourceFile>> sourceFiles(const std::string &path) {
  if (path.empty())
    return std::vector<SourceFile>{};
  return catalog::open(path, false).and_then([](auto configuration) {
    return catalog::files(configuration);
  }).and_then([](auto files) -> catalog::Result<std::vector<SourceFile>> {
    std::vector<SourceFile> sources;
    for (auto &file : files) {
      auto resolvedPath = catalog::filePath(file);
      if (!resolvedPath)
        return std::unexpected(resolvedPath.error());
      sources.push_back(
          {static_cast<FileId>(file.id), std::move(file.name), *resolvedPath});
    }
    return sources;
  });
}

catalog::Result<std::vector<SymbolFact>>
symbols(storage::Database &database, const std::vector<SourceFile> &files,
        const std::optional<std::string> &name) {
  return catalog::query(
      database,
      "SELECT id,node,kind,sub_kind,lang,properties,usr,qualified_name,line,"
      "col,offset,access,is_definition,is_implicit,is_static,is_virtual,"
      "is_const,is_inline,is_pure,ref_qualifier,is_override,"
      "has_internal_linkage,is_external,is_variadic,is_deleted,is_defaulted,"
      "is_explicit,is_final,is_abstract,is_polymorphic,has_extern_storage,"
      "constant_evaluation,is_noexcept FROM symbol "
      "WHERE (? IS NULL OR qualified_name=?) ORDER BY id",
      [&files](const storage::Row &row) {
        SymbolFact value;
        value.id = row.get<SymbolId>(0);
        value.line = row.integer(8);
        value.type = std::string(symbolNodeName(
            static_cast<Storage::SymbolNode>(row.integer(1))));
        value.kind = clang::index::getSymbolKindString(
                         row.get<clang::index::SymbolKind>(2))
                         .str();
        value.details[4] = symbolProperties(row.integer(5));
        for (int column = 6; column <= 32; ++column)
          value.details[static_cast<std::size_t>(column - 1)] =
              symbolDetail(row, column);
        value.details[2] = clang::index::getSymbolSubKindString(
                               row.get<clang::index::SymbolSubKind>(3))
                               .str();
        value.details[3] = clang::index::getSymbolLanguageString(
                               row.get<clang::index::SymbolLanguage>(4))
                               .str();
        value.source = sourceLocation(files, value.id, value.line);
        return value;
      },
      name, name);
}

std::string displaySymbols(const std::vector<SymbolFact> &values) {
  if (values.empty())
    return "No symbols\n";
  std::string output = "ID\tSOURCE\tKIND\tTYPE\tQUALIFIED NAME\tUSR\n";
  for (const auto &value : values)
    output +=
        std::format("{}\t{}\t{}\t{}\t{}\t{}\n", value.id.packed(),
                    value.source, value.kind, value.type, value.details[6],
                    value.details[5]);
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
      std::format("ID: {}\nFILE ID: {}\nSYMBOL INDEX: {}\nSOURCE: {}\n"
                  "KIND: {}\nTYPE: {}\n",
                  value.id.packed(), value.id.file, value.id.index,
                  value.source, value.kind, value.type);
  for (std::size_t index = 2; index < labels.size(); ++index)
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
  const auto name = options.action == cli::SymbolOptions::Action::show
                        ? std::optional{options.qualifiedName}
                        : std::nullopt;
  return sourceFiles(options.configuration).and_then(
      [&](const auto &files) { return symbols(database, files, name); })
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
