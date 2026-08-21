#include "storage/RecordInstance.h"

#include "storage/SemanticProperties.h"
#include "storage/Sqlite.h"

#include <sqlite3.h>

#include <ranges>

namespace facts {

std::expected<void, std::error_code>
Storage::addTemplateParameters(SymbolId id,
                               std::span<const TemplateParameter> parameters) {
  auto transaction = storage::Transaction::write(handle());
  if (!transaction) {
    return std::unexpected(transaction.error());
  }

  constexpr auto sql =
      "INSERT INTO template_parameter(symbol_id,position,value,type_id,"
      "is_pointer,is_lvalue_reference,is_rvalue_reference,"
      "is_forwarding_reference,is_const,is_pack,kind,pack_index) "
      "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12) "
      "ON CONFLICT(symbol_id,position) DO NOTHING";
  const auto insert = [&](std::size_t position) {
    const auto &parameter = parameters[position];
    const auto properties = storage::parameterProperties(
        static_cast<std::uint8_t>(parameter.flags));
    auto statement = storage::prepare(handle(), sql);
    return statement &&
           storage::bindInteger(statement->get(), 1,
                                storage::packSymbolId(id)) &&
           storage::bindInteger(statement->get(), 2, position) &&
           storage::bindText(statement->get(), 3, parameter.value) &&
           storage::bindInteger(statement->get(), 4,
                                storage::packSymbolId(parameter.type)) &&
           storage::bindInteger(statement->get(), 5, properties.isPointer) &&
           storage::bindInteger(statement->get(), 6,
                                properties.isLValueReference) &&
           storage::bindInteger(statement->get(), 7,
                                properties.isRValueReference) &&
           storage::bindInteger(statement->get(), 8,
                                properties.isForwardingReference) &&
           storage::bindInteger(statement->get(), 9, properties.isConst) &&
           storage::bindInteger(statement->get(), 10, properties.isPack) &&
           storage::bindInteger(statement->get(), 11, parameter.kind) &&
           storage::bindInteger(statement->get(), 12, parameter.packIndex) &&
           sqlite3_step(statement->get()) == SQLITE_DONE;
  };

  if (!std::ranges::all_of(std::views::iota(std::size_t{0}, parameters.size()),
                           insert)) {
    return std::unexpected(storage::sqliteError(handle()));
  }
  return transaction->commit();
}

template <>
std::expected<SymbolId, std::error_code>
Storage::save<RecordInstance>(const RecordInstance &record) {
  return saveModel(SymbolNode::Record, record, {.definition = true})
      .and_then([this, &record](SymbolId id) {
        return addTemplateArguments(id, record.templateArguments)
            .and_then([this, &record, id] {
              return addTemplateParameters(id, record.templateParameters);
            })
            .transform([id] { return id; });
      });
}

} // namespace facts
