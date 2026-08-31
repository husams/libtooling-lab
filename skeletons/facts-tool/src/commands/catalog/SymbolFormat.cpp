#include "commands/catalog/SymbolFormat.h"

#include "storage/SqliteQuery.h"
#include "clang/Index/IndexSymbol.h"

#include <algorithm>
#include <array>
#include <format>
#include <string_view>
#include <vector>

namespace facts::commands {

std::vector<std::string> trueFlags(const storage::Row &row) {
  static constexpr std::array flagColumns{std::pair{12, "definition"},
                                          std::pair{13, "implicit"},
                                          std::pair{14, "static"},
                                          std::pair{15, "virtual"},
                                          std::pair{16, "const"},
                                          std::pair{17, "inline"},
                                          std::pair{18, "pure"},
                                          std::pair{20, "override"},
                                          std::pair{21, "internal-linkage"},
                                          std::pair{22, "external"},
                                          std::pair{23, "variadic"},
                                          std::pair{24, "deleted"},
                                          std::pair{25, "defaulted"},
                                          std::pair{26, "explicit"},
                                          std::pair{27, "final"},
                                          std::pair{28, "abstract"},
                                          std::pair{29, "polymorphic"},
                                          std::pair{30, "extern-storage"},
                                          std::pair{32, "noexcept"}};
  std::vector<std::string> flags;
  for (const auto &[column, name] : flagColumns)
    if (row.integer(column))
      flags.emplace_back(name);
  return flags;
}

namespace {

constexpr std::string_view
symbolPropertyName(clang::index::SymbolProperty property) noexcept {
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

std::string renderParameter(const ParameterFact &parameter) {
  auto result = parameter.type.empty()
                    ? std::format("<symbol {}>", symbolId(parameter.typeId))
                    : parameter.type;
  if (parameter.constant)
    result = "const " + result;
  if (parameter.pointer)
    result += "*";
  if (parameter.lvalueReference)
    result += "&";
  if (parameter.rvalueReference)
    result += "&&";
  if (parameter.pack)
    result += "...";
  if (!parameter.name.empty())
    result += " " + parameter.name;
  if (parameter.hasDefault)
    result += " = " + parameter.defaultValue;
  return result;
}

bool hasFlag(const SymbolFact &value, std::string_view flag) {
  return std::ranges::find(value.flags, flag) != value.flags.end();
}

std::string declaration(const SymbolFact &value) {
  auto result = value.qualifiedName;
  if (value.type == "Function") {
    std::vector<std::string> parameters;
    parameters.reserve(value.parameters.size());
    for (const auto &parameter : value.parameters)
      parameters.push_back(renderParameter(parameter));
    result += "(" + join(parameters) + ")";
    if (hasFlag(value, "const"))
      result += " const";
    if (value.refQualifier == "lvalue" || value.refQualifier == "&")
      result += " &";
    if (value.refQualifier == "rvalue" || value.refQualifier == "&&")
      result += " &&";
    if (hasFlag(value, "noexcept"))
      result += " noexcept";
  }
  if (!value.returnType.empty())
    result += " -> " + value.returnType;
  return result;
}

std::string formatListRow(const std::array<std::string, 2> &row,
                          std::size_t kindWidth) {
  return std::format("{:<{}} {}\n", row[0], kindWidth, row[1]);
}

} // namespace

std::string symbolDeclaration(const SymbolFact &value) {
  return declaration(value);
}

std::string displaySymbols(const std::vector<SymbolFact> &values) {
  if (values.empty())
    return "No symbols\n";
  const std::array<std::string, 2> headers{"kind", "qualified name"};
  std::vector<std::array<std::string, 2>> rows;
  rows.reserve(values.size());
  for (const auto &value : values)
    rows.push_back({value.kind, value.qualifiedName});
  auto kindWidth = headers[0].size();
  for (const auto &row : rows)
    kindWidth = std::max(kindWidth, row[0].size());
  std::string output = formatListRow(headers, kindWidth);
  for (const auto &row : rows)
    output += formatListRow(row, kindWidth);
  return output;
}

std::string displaySymbol(const SymbolFact &value) {
  std::string output = symbolDeclaration(value);
  output += std::format("\n  identity   {}\n  kind       {}\n  type       {}\n",
                        symbolId(value.id), value.kind, value.type);
  if (value.subKind != "none")
    output += std::format("  sub-kind   {}\n", value.subKind);
  if (value.language != "none")
    output += std::format("  language   {}\n", value.language);
  if (value.access != "none")
    output += std::format("  access     {}\n", value.access);
  output += std::format("  source     {}:{}:{}\n", value.sourceName, value.line,
                        value.column);
  if (!value.sourcePath.empty())
    output += std::format("             {}\n", value.sourcePath.string());
  output += std::format("  usr        {}\n  properties {}\n", value.usr,
                        symbolProperties(value.properties));
  if (!value.flags.empty())
    output += std::format("  flags      {}\n", join(value.flags));
  if (value.refQualifier != "none")
    output += std::format("  ref        {}\n", value.refQualifier);
  if (value.constantEvaluation != "none")
    output += std::format("  constant   {}\n", value.constantEvaluation);
  return output;
}

} // namespace facts::commands
