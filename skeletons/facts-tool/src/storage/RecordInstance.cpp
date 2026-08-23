#include "storage/RecordInstance.h"

#include "storage/SemanticProperties.h"

#include <ranges>

namespace facts {

std::expected<void, std::error_code>
Storage::addTemplateParameters(SymbolId id,
                               std::span<const TemplateParameter> parameters) {
  constexpr auto sql =
      "INSERT INTO template_parameter(symbol_id,position,value,type_id,"
      "is_pointer,is_lvalue_reference,is_rvalue_reference,"
      "is_forwarding_reference,is_const,is_pack,kind,pack_index) "
      "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12) "
      "ON CONFLICT(symbol_id,position) DO NOTHING";
  auto positions = std::views::iota(std::size_t{0}, parameters.size());
  return database_
      .executeBulk(
          sql, positions,
          [id, parameters](sqlite3_stmt *statement, std::size_t position) {
            const auto &parameter = parameters[position];
            const auto properties = storage::parameterProperties(
                static_cast<std::uint8_t>(parameter.flags));
            return storage::bindParameters(
                statement, id, position, parameter.value, parameter.type,
                properties.isPointer, properties.isLValueReference,
                properties.isRValueReference, properties.isForwardingReference,
                properties.isConst, properties.isPack, parameter.kind,
                parameter.packIndex);
          })
      .transform([](const storage::BulkResult &) {});
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
