#include "commands/catalog/SymbolData.h"
#include "commands/catalog/SymbolFormat.h"
#include "storage/Sqlite.h"
#include "storage/SqliteDatabase.h"
#include "storage/Storage.h"
#include "storage/catalog/File.h"

#include "clang/AST/Type.h"
#include "clang/Index/IndexSymbol.h"

#include <algorithm>
#include <format>
#include <utility>

namespace facts::commands {
namespace {

struct SourceFile {
  FileId id;
  std::string name;
  std::filesystem::path path;
};

std::string normalizeNone(std::string value) {
  return value == "<none>" ? "none" : std::move(value);
}

std::string builtinTypeName(SymbolId id) {
  if (id.file != builtinFileId)
    return "";
  using BuiltinType = clang::BuiltinType;
  switch (id.index) {
  case static_cast<std::uint32_t>(BuiltinType::Void) + 1:
    return "void";
  case static_cast<std::uint32_t>(BuiltinType::Bool) + 1:
    return "bool";
  case static_cast<std::uint32_t>(BuiltinType::Char_U) + 1:
  case static_cast<std::uint32_t>(BuiltinType::Char_S) + 1:
    return "char";
  case static_cast<std::uint32_t>(BuiltinType::SChar) + 1:
    return "signed char";
  case static_cast<std::uint32_t>(BuiltinType::UChar) + 1:
    return "unsigned char";
  case static_cast<std::uint32_t>(BuiltinType::Short) + 1:
    return "short";
  case static_cast<std::uint32_t>(BuiltinType::UShort) + 1:
    return "unsigned short";
  case static_cast<std::uint32_t>(BuiltinType::Int) + 1:
    return "int";
  case static_cast<std::uint32_t>(BuiltinType::UInt) + 1:
    return "unsigned int";
  case static_cast<std::uint32_t>(BuiltinType::Long) + 1:
    return "long";
  case static_cast<std::uint32_t>(BuiltinType::ULong) + 1:
    return "unsigned long";
  case static_cast<std::uint32_t>(BuiltinType::LongLong) + 1:
    return "long long";
  case static_cast<std::uint32_t>(BuiltinType::ULongLong) + 1:
    return "unsigned long long";
  case static_cast<std::uint32_t>(BuiltinType::Float) + 1:
    return "float";
  case static_cast<std::uint32_t>(BuiltinType::Double) + 1:
    return "double";
  case static_cast<std::uint32_t>(BuiltinType::LongDouble) + 1:
    return "long double";
  default:
    return "";
  }
}

catalog::Result<std::vector<SourceFile>> sourceFiles(const std::string &path) {
  if (path.empty())
    return std::vector<SourceFile>{};
  return catalog::open(path, false)
      .and_then(
          [](auto configuration) { return catalog::files(configuration); })
      .and_then([](auto files) -> catalog::Result<std::vector<SourceFile>> {
        std::vector<SourceFile> sources;
        sources.reserve(files.size());
        for (auto &file : files) {
          auto resolvedPath = catalog::filePath(file);
          if (!resolvedPath)
            return std::unexpected(resolvedPath.error());
          sources.push_back({static_cast<FileId>(file.id), std::move(file.name),
                             *resolvedPath});
        }
        return sources;
      });
}

catalog::Result<std::vector<ParameterFact>>
loadParameters(storage::Database &database) {
  return catalog::query(
      database,
      "SELECT p.symbol_id,p.name,p.type,type.qualified_name,"
      "p.is_pointer,p.is_lvalue_reference,p.is_rvalue_reference,"
      "p.is_const,p.is_pack,p.has_default,d.expression "
      "FROM parameter p LEFT JOIN symbol type ON type.id=p.type "
      "LEFT JOIN parameter_default d ON d.symbol_id=p.symbol_id "
      "AND d.position=p.position ORDER BY p.symbol_id,p.position",
      [](const storage::Row &row) {
        ParameterFact value;
        value.symbol = row.get<SymbolId>(0);
        value.typeId = row.get<SymbolId>(2);
        value.name = row.string(1);
        value.type =
            row.isNull(3) ? builtinTypeName(value.typeId) : row.string(3);
        value.pointer = row.integer(4);
        value.lvalueReference = row.integer(5);
        value.rvalueReference = row.integer(6);
        value.constant = row.integer(7);
        value.pack = row.integer(8);
        value.hasDefault = row.integer(9);
        value.defaultValue = row.isNull(10) ? "" : row.string(10);
        return value;
      });
}

std::vector<SymbolFact>
attachParameters(std::vector<SymbolFact> values,
                 const std::vector<ParameterFact> &parameters) {
  for (auto &value : values)
    for (const auto &parameter : parameters)
      if (parameter.symbol == value.id)
        value.parameters.push_back(parameter);
  return values;
}

catalog::Result<std::vector<SymbolFact>>
querySymbols(storage::Database &database, const std::vector<SourceFile> &files,
             const std::optional<std::string> &name) {
  return catalog::query(
             database,
             "SELECT "
             "id,node,kind,sub_kind,lang,properties,usr,qualified_name,line,"
             "col,offset,access,is_definition,is_implicit,is_static,is_virtual,"
             "is_const,is_inline,is_pure,ref_qualifier,is_override,"
             "has_internal_linkage,is_external,is_variadic,is_deleted,is_"
             "defaulted,"
             "is_explicit,is_final,is_abstract,is_polymorphic,has_extern_"
             "storage,"
             "constant_evaluation,is_noexcept,"
             "(SELECT destination.qualified_name FROM relation "
             "JOIN symbol destination ON "
             "destination.id=relation.destination_id "
             "WHERE relation.source_id=s.id AND relation.kind=? LIMIT 1) "
             "FROM symbol s WHERE (? IS NULL OR qualified_name=?) ORDER BY id",
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
               value.subKind =
                   normalizeNone(clang::index::getSymbolSubKindString(
                                     row.get<clang::index::SymbolSubKind>(3))
                                     .str());
               value.language =
                   normalizeNone(clang::index::getSymbolLanguageString(
                                     row.get<clang::index::SymbolLanguage>(4))
                                     .str());
               value.access = row.string(11);
               value.properties = row.integer(5);
               value.refQualifier = row.string(19);
               value.constantEvaluation = row.string(31);
               value.returnType = row.isNull(33) ? "" : row.string(33);
               value.flags = trueFlags(row);
               const auto file =
                   std::ranges::find(files, value.id.file, &SourceFile::id);
               if (file == files.end())
                 value.sourceName = std::format("<file {}>", value.id.file);
               else {
                 value.sourceName = file->name;
                 value.sourcePath = file->path;
               }
               return value;
             },
             static_cast<int>(RelationKind::ReturnType), name, name)
      .and_then([&database](auto values) {
        return loadParameters(database).transform(
            [&values](const auto &parameters) mutable {
              return attachParameters(std::move(values), parameters);
            });
      });
}

catalog::Result<storage::Database> openFacts(const std::string &path) {
  return storage::Database::open(path, storage::Database::readOnly)
      .transform_error([](const auto &error) {
        return "cannot open facts database: " + error.message();
      });
}

} // namespace

catalog::Result<std::vector<SymbolFact>>
loadSymbols(const std::string &factsPath, const std::string &configurationPath,
            const std::optional<std::string> &qualifiedName) {
  return openFacts(factsPath).and_then([&](auto database) {
    return sourceFiles(configurationPath).and_then([&](const auto &files) {
      return querySymbols(database, files, qualifiedName);
    });
  });
}

} // namespace facts::commands
