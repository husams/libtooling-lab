#include "storage/Initializer.h"

#include <string_view>

namespace facts::storage {
namespace {

std::string_view valueKindName(EvaluatedValueKind kind) {
  switch (kind) {
  case EvaluatedValueKind::Integer:
    return "integer";
  case EvaluatedValueKind::Floating:
    return "floating";
  case EvaluatedValueKind::Boolean:
    return "boolean";
  case EvaluatedValueKind::String:
    return "string";
  }
  return "none";
}

std::optional<EvaluatedValueKind> valueKind(std::string_view name) {
  if (name == "integer") {
    return EvaluatedValueKind::Integer;
  }
  if (name == "floating") {
    return EvaluatedValueKind::Floating;
  }
  if (name == "boolean") {
    return EvaluatedValueKind::Boolean;
  }
  if (name == "string") {
    return EvaluatedValueKind::String;
  }
  return std::nullopt;
}

} // namespace

InitializerColumns initializerColumns(const Initializer &initializer) {
  const auto kind = initializer.evaluated
                        ? valueKindName(initializer.evaluated->kind)
                        : std::string_view{"none"};
  return {
      .expression = initializer.expression,
      .kind = std::string{kind},
      .value = initializer.evaluated
                   ? std::optional<std::string>{initializer.evaluated->value}
                   : std::nullopt,
  };
}

std::optional<Initializer> loadInitializer(const Row &row, int expressionColumn,
                                           int kindColumn, int valueColumn) {
  if (row.isNull(expressionColumn)) {
    return std::nullopt;
  }

  Initializer initializer{
      .expression = row.get<std::string>(expressionColumn),
  };
  const auto kind = valueKind(row.get<std::string>(kindColumn));
  if (kind) {
    initializer.evaluated = EvaluatedValue{
        .kind = *kind,
        .value = row.get<std::optional<std::string>>(valueColumn).value_or(""),
    };
  }
  return initializer;
}

} // namespace facts::storage
