#pragma once

#include "model/SymbolId.h"
#include "storage/catalog/Database.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace facts::commands {

struct ParameterFact {
  SymbolId symbol;
  SymbolId typeId;
  std::string name;
  std::string type;
  bool pointer = false;
  bool lvalueReference = false;
  bool rvalueReference = false;
  bool constant = false;
  bool pack = false;
  bool hasDefault = false;
  std::string defaultValue;
};

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
  std::int64_t properties;
  std::string refQualifier;
  std::string constantEvaluation;
  std::string returnType;
  std::vector<std::string> flags;
  std::string sourceName;
  std::filesystem::path sourcePath;
  std::vector<ParameterFact> parameters;
};

catalog::Result<std::vector<SymbolFact>>
loadSymbols(const std::string &factsPath, const std::string &configurationPath,
            const std::optional<std::string> &qualifiedName = std::nullopt);

} // namespace facts::commands
