#include "storage/RecordTemplate.h"

#include "storage/StorageQuery.h"

#include <ranges>

namespace facts {

std::expected<void, std::error_code>
Storage::addTemplateArguments(SymbolId id,
                              std::span<const TemplateArgument> arguments) {
  constexpr auto sql =
      "INSERT INTO template_argument(symbol_id,position,name,type_id,"
      "is_parameter_pack,is_non_type,is_template_template) "
      "VALUES(?1,?2,?3,?4,?5,?6,?7) "
      "ON CONFLICT(symbol_id,position) DO NOTHING";
  auto positions = std::views::iota(std::size_t{0}, arguments.size());
  return database_
      .executeBulk(sql, positions,
                   storage::detail::typedBinder(
                       [id, arguments](auto bind, std::size_t position) {
                         const auto &argument = arguments[position];
                         return bind(
                             id, position, argument.name, argument.type,
                             (argument.flags & bit(ParameterPackBit)) != 0,
                             (argument.flags & bit(NonTypeBit)) != 0,
                             (argument.flags & bit(TemplateTemplateBit)) != 0);
                       }))
      .transform([](const storage::BulkResult &) {});
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
