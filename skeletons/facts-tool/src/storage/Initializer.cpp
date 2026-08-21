#include "storage/Initializer.h"

#include "storage/Sqlite.h"

#include <sqlite3.h>

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

bool bindInitializer(sqlite3_stmt *statement, int expressionPosition,
                     int kindPosition, int valuePosition,
                     const Initializer &initializer) {
  const auto kind = initializer.evaluated
                        ? valueKindName(initializer.evaluated->kind)
                        : std::string_view{"none"};
  const auto valueBound =
      initializer.evaluated
          ? bindText(statement, valuePosition, initializer.evaluated->value)
          : sqlite3_bind_null(statement, valuePosition) == SQLITE_OK;
  return bindText(statement, expressionPosition, initializer.expression) &&
         bindText(statement, kindPosition, kind) && valueBound;
}

std::optional<Initializer> loadInitializer(sqlite3_stmt *statement,
                                           int expressionColumn, int kindColumn,
                                           int valueColumn) {
  if (sqlite3_column_type(statement, expressionColumn) == SQLITE_NULL) {
    return std::nullopt;
  }

  Initializer initializer{
      .expression = columnText(statement, expressionColumn),
  };
  const auto kind = valueKind(columnText(statement, kindColumn));
  if (kind) {
    initializer.evaluated = EvaluatedValue{
        .kind = *kind,
        .value = columnText(statement, valueColumn),
    };
  }
  return initializer;
}

} // namespace facts::storage
