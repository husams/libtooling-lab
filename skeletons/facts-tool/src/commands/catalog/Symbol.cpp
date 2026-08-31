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
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace facts::commands {
namespace {

struct SymbolFact {
  SymbolId id;
  std::int64_t line;
  std::int64_t column;
  std::string qualifiedName;
  std::string usr;
  std::string type;
  std::string kind;
  std::string subKind;
  std::string language;
  std::string access;
  std::string properties;
  std::string refQualifier;
  std::string constantEvaluation;
  std::string returnType;
  std::vector<std::string> flags;
  std::string sourceName;
  std::filesystem::path sourcePath;
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

std::string normalizeNone(std::string value) {
  return value == "<none>" ? "none" : value;
}

std::vector<std::string> trueFlags(const storage::Row &row) {
  static constexpr std::array flagColumns{
      std::pair{12, "definition"}, std::pair{13, "implicit"},
      std::pair{14, "static"}, std::pair{15, "virtual"},
      std::pair{16, "const"}, std::pair{17, "inline"},
      std::pair{18, "pure"}, std::pair{20, "override"},
      std::pair{21, "internal-linkage"}, std::pair{22, "external"},
      std::pair{23, "variadic"}, std::pair{24, "deleted"},
      std::pair{25, "defaulted"}, std::pair{26, "explicit"},
      std::pair{27, "final"}, std::pair{28, "abstract"},
      std::pair{29, "polymorphic"}, std::pair{30, "extern-storage"},
      std::pair{32, "noexcept"}};
  std::vector<std::string> flags;
  for (const auto &[column, name] : flagColumns)
    if (row.integer(column))
      flags.emplace_back(name);
  return flags;
}

std::string join(const std::vector<std::string> &values) {
  std::string result;
  for (const auto &value : values) {
    if (!result.empty())
      result += ", ";
    result += value;
  }
  return result;
}

std::string symbolId(SymbolId id) {
  return std::format("{}:{}", id.file, id.index);
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
      "constant_evaluation,is_noexcept,"
      "(SELECT destination.qualified_name FROM relation "
      "JOIN symbol destination ON destination.id=relation.destination_id "
      "WHERE relation.source_id=symbol.id AND relation.kind=? LIMIT 1) "
      "FROM symbol WHERE (? IS NULL OR qualified_name=?) ORDER BY id",
      [&files](const storage::Row &row) {
        SymbolFact value;
        value.id = row.get<SymbolId>(0);
        value.line = row.integer(8);
        value.column = row.integer(9);
        value.qualifiedName = row.string(7);
        value.usr = row.string(6);
        value.type = std::string(symbolNodeName(
            static_cast<Storage::SymbolNode>(row.integer(1))));
        value.kind = clang::index::getSymbolKindString(
                         row.get<clang::index::SymbolKind>(2))
                         .str();
        value.subKind = normalizeNone(
            clang::index::getSymbolSubKindString(
                row.get<clang::index::SymbolSubKind>(3))
                .str());
        value.language = normalizeNone(
            clang::index::getSymbolLanguageString(
                row.get<clang::index::SymbolLanguage>(4))
                .str());
        value.access = row.string(11);
        value.properties = symbolProperties(row.integer(5));
        value.refQualifier = row.string(19);
        value.constantEvaluation = row.string(31);
        value.returnType = row.isNull(33) ? "" : row.string(33);
        value.flags = trueFlags(row);
        const auto file = std::ranges::find(files, value.id.file,
                                            &SourceFile::id);
        if (file == files.end())
          value.sourceName = std::format("<file {}>", value.id.file);
        else {
          value.sourceName = file->name;
          value.sourcePath = file->path;
        }
        return value;
      },
      static_cast<int>(RelationKind::ReturnType), name, name);
}

std::string displaySymbols(const std::vector<SymbolFact> &values) {
  if (values.empty())
    return "No symbols\n";
  std::string output = "id\tsymbol\tkind\tflags\tlocation\n";
  for (const auto &value : values)
    output += std::format("{}\t{}\t{}\t{}\t{}:{}\n", symbolId(value.id),
                          value.qualifiedName, value.kind,
                          value.flags.empty() ? "-" : join(value.flags),
                          value.sourceName, value.line);
  return output;
}

std::string displaySymbol(const SymbolFact &value) {
  std::string output = value.qualifiedName;
  if (!value.returnType.empty())
    output += std::format(" -> {}", value.returnType);
  output += std::format("\n  identity   {}\n  kind       {}\n  type       {}\n",
                        symbolId(value.id), value.kind, value.type);
  if (value.subKind != "none")
    output += std::format("  sub-kind   {}\n", value.subKind);
  if (value.language != "none")
    output += std::format("  language   {}\n", value.language);
  if (value.access != "none")
    output += std::format("  access     {}\n", value.access);
  output += std::format("  source     {}:{}:{}\n", value.sourceName,
                        value.line, value.column);
  if (!value.sourcePath.empty())
    output += std::format("             {}\n", value.sourcePath.string());
  output += std::format("  usr        {}\n  properties {}\n", value.usr,
                        value.properties);
  if (!value.flags.empty())
    output += std::format("  flags      {}\n", join(value.flags));
  if (value.refQualifier != "none")
    output += std::format("  ref        {}\n", value.refQualifier);
  if (value.constantEvaluation != "none")
    output += std::format("  constant   {}\n", value.constantEvaluation);
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
