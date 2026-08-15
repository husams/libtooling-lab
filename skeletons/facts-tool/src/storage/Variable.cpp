#include "storage/Variable.h"

#include "storage/Sqlite.h"

#include <sqlite3.h>

#include <string_view>

namespace facts {
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

std::expected<void, std::error_code> Storage::replaceVariableInitializer(
    SymbolId id, const std::optional<VariableInitializer> &initializer) {
  if (!initializer) {
    return {};
  }

  const auto kind = initializer->evaluated
                        ? valueKindName(initializer->evaluated->kind)
                        : std::string_view{"none"};
  auto statement = storage::prepare(
      handle(),
      "INSERT INTO variable_initializer(symbol_id,expression,evaluated_kind,"
      "evaluated_value) VALUES(?1,?2,?3,?4) "
      "ON CONFLICT(symbol_id) DO UPDATE SET expression=excluded.expression,"
      "evaluated_kind=excluded.evaluated_kind,"
      "evaluated_value=excluded.evaluated_value");
  if (!statement ||
      !storage::bindInteger(statement->get(), 1, storage::packSymbolId(id)) ||
      !storage::bindText(statement->get(), 2, initializer->expression) ||
      !storage::bindText(statement->get(), 3, kind)) {
    return std::unexpected(storage::sqliteError(handle()));
  }

  const auto valueBound =
      initializer->evaluated
          ? storage::bindText(statement->get(), 4,
                              initializer->evaluated->value)
          : sqlite3_bind_null(statement->get(), 4) == SQLITE_OK;
  if (!valueBound || sqlite3_step(statement->get()) != SQLITE_DONE) {
    return std::unexpected(storage::sqliteError(handle()));
  }
  return {};
}

std::expected<std::optional<VariableInitializer>, std::error_code>
Storage::loadVariableInitializer(SymbolId id) {
  auto statement = storage::prepare(
      handle(), "SELECT expression,evaluated_kind,evaluated_value "
                "FROM variable_initializer WHERE symbol_id=?1");
  if (!statement ||
      !storage::bindInteger(statement->get(), 1, storage::packSymbolId(id))) {
    return std::unexpected(storage::sqliteError(handle()));
  }

  const auto step = sqlite3_step(statement->get());
  if (step == SQLITE_DONE) {
    return std::nullopt;
  }
  if (step != SQLITE_ROW) {
    return std::unexpected(storage::sqliteError(handle()));
  }

  VariableInitializer initializer{
      .expression = storage::columnText(statement->get(), 0),
  };
  const auto kind = valueKind(storage::columnText(statement->get(), 1));
  if (kind) {
    initializer.evaluated = EvaluatedValue{
        .kind = *kind,
        .value = storage::columnText(statement->get(), 2),
    };
  }
  return std::optional{std::move(initializer)};
}

template <>
std::expected<SymbolId, std::error_code>
Storage::save<Variable>(const Variable &variable) {
  return saveModel(SymbolNode::Variable, variable, {.definition = true})
      .and_then([this, &variable](SymbolId id) {
        return replaceVariableInitializer(id, variable.initializer)
            .transform([id] { return id; });
      });
}

template <>
std::expected<Variable, std::error_code> Storage::load<Variable>(SymbolId id) {
  return loadModel<Variable>(SymbolNode::Variable, id, {.definition = true})
      .and_then([this](Variable variable) {
        return loadVariableInitializer(variable.id)
            .transform(
                [variable = std::move(variable)](
                    std::optional<VariableInitializer> initializer) mutable {
                  variable.initializer = std::move(initializer);
                  return variable;
                });
      });
}

template <>
std::expected<std::optional<Variable>, std::error_code>
Storage::load<Variable>(std::string_view usr) {
  return loadModel<Variable>(SymbolNode::Variable, usr, {.definition = true})
      .and_then([this](std::optional<Variable> variable)
                    -> std::expected<std::optional<Variable>, std::error_code> {
        if (!variable) {
          return std::nullopt;
        }
        return loadVariableInitializer(variable->id)
            .transform(
                [variable = std::move(*variable)](
                    std::optional<VariableInitializer> initializer) mutable {
                  variable.initializer = std::move(initializer);
                  return std::optional{std::move(variable)};
                });
      });
}

} // namespace facts
