#ifndef FACTS_TOOL_STORAGE_FACT_STORE_H
#define FACTS_TOOL_STORAGE_FACT_STORE_H

#include "cli/Trace.h"
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
  explicit FactStore(std::string path, int verbosity = 0);

  std::expected<void, std::error_code> begin();
  std::expected<void, std::error_code> end();
  std::expected<void, std::error_code> rollback();

  template <typename Model>
    requires std::derived_from<Model, Symbol>
  std::expected<SymbolId, std::error_code> save(const Model &object) {
    return storage_.save<Model>(object)
        .transform([this, &object](SymbolId id) {
          remember(object.usr, id);
          cli::traceSymbol(verbosity_, object, id);
          return id;
        })
        .transform_error([this, &object](std::error_code error) {
          cli::logVerbose(
              verbosity_, 3,
              "facts-tool: trace: symbol persistence result=failure "
              "name='{}' usr='{}' error='{}'",
              object.qualifiedName, object.usr, error.message());
          return error;
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
    cli::traceRelations(verbosity_, "add-relations", relations);
    auto result = storage_.addRelations(relations);
    cli::tracePersistenceResult(verbosity_, "add-relations", result);
    return result;
  }

  std::expected<void, std::error_code>
  addRelationSites(std::span<const RelationSite> sites) {
    cli::traceRelationSites(verbosity_, sites);
    auto result = storage_.addRelationSites(sites);
    cli::tracePersistenceResult(verbosity_, "add-relation-sites", result);
    return result;
  }

  std::expected<void, std::error_code>
  addUseFacts(std::span<const Relation> relations,
              std::span<const RelationSite> sites) {
    cli::traceRelations(verbosity_, "add-use-facts", relations);
    cli::traceRelationSites(verbosity_, sites);
    auto result = storage_.addUseFacts(relations, sites);
    cli::tracePersistenceResult(verbosity_, "add-use-facts", result);
    return result;
  }

  std::expected<void, std::error_code>
  addRelationFacts(std::span<const Relation> relations,
                   std::span<const RelationSite> sites) {
    cli::traceRelations(verbosity_, "add-relation-facts", relations);
    cli::traceRelationSites(verbosity_, sites);
    auto result = storage_.addRelationFacts(relations, sites);
    cli::tracePersistenceResult(verbosity_, "add-relation-facts", result);
    return result;
  }

  std::expected<void, std::error_code>
  addTemplateArguments(SymbolId id,
                       std::span<const TemplateArgument> arguments) {
    cli::logVerbose(
        verbosity_, 3,
        "facts-tool: trace: template-argument batch symbol={}:{} count={}",
        id.file, id.index, arguments.size());
    for (std::size_t position = 0; position < arguments.size(); ++position) {
      const auto &argument = arguments[position];
      cli::logVerbose(
          verbosity_, 3,
          "facts-tool: trace: template-argument symbol={}:{} position={} "
          "name='{}' type={}:{} flags={}",
          id.file, id.index, position, argument.name, argument.type.file,
          argument.type.index, argument.flags);
    }
    auto result = storage_.addTemplateArguments(id, arguments);
    cli::tracePersistenceResult(verbosity_, "add-template-arguments", result);
    return result;
  }

  bool contains(std::string_view usr) const;

  std::size_t cachedIdCount() const { return idsByUsr_.size(); }

  unsigned count() const { return static_cast<unsigned>(idsByUsr_.size()); }

  int verbosity() const { return verbosity_; }

private:
  void remember(std::string_view usr, SymbolId id);

  Storage storage_;
  std::unordered_map<std::string, SymbolId> idsByUsr_;
  int verbosity_ = 0;
};

} // namespace facts

#endif
