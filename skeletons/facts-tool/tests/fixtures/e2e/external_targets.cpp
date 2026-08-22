#include <cstdint>
#include <expected>
#include <string_view>

namespace regression {

struct ExternalValues {
  std::expected<int, long> result;
  std::string_view view;
};

using RelationResult = std::expected<int, long>;

void inheritanceFailure(std::string_view value) { (void)value; }

void externalSymbol(const std::expected<int, long> *value) { (void)value; }

void findOrStoreInheritanceTarget(const std::expected<int, long> &value) {
  (void)value;
}

void extractInheritanceRelation(std::expected<int, long> &&value,
                                std::uint16_t position) {
  (void)value;
  (void)position;
}

} // namespace regression
