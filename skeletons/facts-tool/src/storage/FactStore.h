#ifndef FACTS_TOOL_STORAGE_FACT_STORE_H
#define FACTS_TOOL_STORAGE_FACT_STORE_H

#include "storage/Storage.h"

#include <concepts>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <variant>

namespace facts {

class FactStore {
public:
  explicit FactStore(std::string path);

  void begin();
  void end();

  template <typename Model>
    requires std::derived_from<Model, Symbol>
  std::expected<SymbolId, std::error_code> save(const Model &object) {
    return storage_.save<Model>(object).transform([this, &object](SymbolId id) {
      remember(object.usr, id);
      return id;
    });
  }

  template <typename Model>
    requires std::derived_from<Model, Symbol>
  std::expected<SymbolId, std::error_code> save(FileId file, Model object) {
    object.id.file = file;
    return save<Model>(object);
  }

  template <typename... Models>
  std::expected<SymbolId, std::error_code>
  save(const std::variant<Models...> &object) {
    return std::visit([this](const auto &value) { return save(value); },
                      object);
  }

  template <typename... Models>
  std::expected<SymbolId, std::error_code>
  save(FileId file, std::variant<Models...> object) {
    return std::visit(
        [this, file](auto value) { return save(file, std::move(value)); },
        std::move(object));
  }

  template <typename Model>
  std::expected<Model, std::error_code> load(SymbolId id) {
    return storage_.load<Model>(id).transform([this](Model model) {
      remember(model.usr, model.id);
      return model;
    });
  }

  template <typename Model>
  std::expected<std::optional<Model>, std::error_code>
  load(std::string_view usr) {
    return findId(usr).and_then(
        [this](std::optional<SymbolId> id)
            -> std::expected<std::optional<Model>, std::error_code> {
          if (!id) {
            return std::nullopt;
          }
          return load<Model>(*id).transform([](Model model) {
            return std::optional<Model>{std::move(model)};
          });
        });
  }

  std::expected<std::optional<SymbolId>, std::error_code>
  findId(std::string_view usr);

  std::expected<void, std::error_code>
  addRelations(std::span<const Relation> relations) {
    return storage_.addRelations(relations);
  }

  std::expected<void, std::error_code>
  addTemplateArguments(SymbolId id,
                       std::span<const TemplateArgument> arguments) {
    return storage_.addTemplateArguments(id, arguments);
  }

  bool contains(std::string_view usr) const;

  std::size_t cachedIdCount() const { return idsByUsr_.size(); }

  unsigned count() const { return static_cast<unsigned>(idsByUsr_.size()); }

private:
  void remember(std::string_view usr, SymbolId id);

  Storage storage_;
  std::unordered_map<std::string, SymbolId> idsByUsr_;
};

} // namespace facts

#endif
