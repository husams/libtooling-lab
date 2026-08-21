#include "storage/RecordTemplate.h"

#include "storage/Sqlite.h"

#include <sqlite3.h>

#include <ranges>

namespace facts {

std::expected<void, std::error_code>
Storage::addTemplateArguments(SymbolId id,
                              std::span<const TemplateArgument> arguments) {
  auto transaction = storage::Transaction::write(handle());
  if (!transaction) {
    return std::unexpected(transaction.error());
  }

  constexpr auto sql =
      "INSERT INTO template_argument(symbol_id,position,name,type_id,"
      "is_parameter_pack,is_non_type,is_template_template) "
      "VALUES(?1,?2,?3,?4,?5,?6,?7) "
      "ON CONFLICT(symbol_id,position) DO NOTHING";
  const auto insert = [&](std::size_t position) {
    const auto &argument = arguments[position];
    auto statement = storage::prepare(handle(), sql);
    return statement &&
           storage::bindInteger(statement->get(), 1,
                                storage::packSymbolId(id)) &&
           storage::bindInteger(statement->get(), 2, position) &&
           storage::bindText(statement->get(), 3, argument.name) &&
           storage::bindInteger(statement->get(), 4,
                                storage::packSymbolId(argument.type)) &&
           storage::bindInteger(statement->get(), 5,
                                (argument.flags & bit(ParameterPackBit)) !=
                                    0) &&
           storage::bindInteger(statement->get(), 6,
                                (argument.flags & bit(NonTypeBit)) != 0) &&
           storage::bindInteger(statement->get(), 7,
                                (argument.flags & bit(TemplateTemplateBit)) !=
                                    0) &&
           sqlite3_step(statement->get()) == SQLITE_DONE;
  };

  if (!std::ranges::all_of(std::views::iota(std::size_t{0}, arguments.size()),
                           insert)) {
    return std::unexpected(storage::sqliteError(handle()));
  }
  return transaction->commit();
}

template <>
std::expected<SymbolId, std::error_code>
Storage::save<RecordTemplate>(const RecordTemplate &record) {
  return saveModel(SymbolNode::Record, record, {.definition = true})
      .and_then([this, &record](SymbolId id) {
        return addTemplateArguments(id, record.templateArguments)
            .transform([id] { return id; });
      });
}

} // namespace facts
