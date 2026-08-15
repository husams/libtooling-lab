#ifndef FACTS_TOOL_STORAGE_STORAGE_H
#define FACTS_TOOL_STORAGE_STORAGE_H

#include "model/AnySymbol.h"
#include "model/Relation.h"
#include "model/TemplateArgument.h"
#include "model/TemplateParameter.h"

#include <concepts>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

struct sqlite3;

namespace facts {

class Storage {
public:
  explicit Storage(std::string path);
  ~Storage();

  Storage(const Storage &) = delete;
  Storage &operator=(const Storage &) = delete;

  template <typename Model>
  std::expected<SymbolId, std::error_code> save(const Model &object);

  template <typename Model>
  std::expected<Model, std::error_code> load(SymbolId id);

  template <typename Model>
  std::expected<std::optional<Model>, std::error_code>
  load(std::string_view usr);

  std::expected<std::optional<SymbolId>, std::error_code>
  findId(std::string_view usr);

  std::expected<void, std::error_code>
  addRelations(std::span<const Relation> relations);
  std::expected<void, std::error_code>
  addTemplateArguments(SymbolId id,
                       std::span<const TemplateArgument> arguments);

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

private:
  enum class SymbolNode : std::uint8_t {
    Function = 1,
    Record,
    Enumeration,
    Variable,
    Symbol,
    Enumerator,
  };

  struct SymbolFacts {
    bool definition = false;
    bool parameters = false;
  };

  struct DefinitionFacts {
    FileId file;
    Region region;
  };

  struct Connection;

  sqlite3 *handle() const;

  std::expected<SymbolId, std::error_code>
  saveSymbol(SymbolNode node, const Symbol &symbol, SymbolFacts facts);
  std::expected<Symbol, std::error_code>
  loadSymbol(SymbolNode node, SymbolId id, SymbolFacts facts);
  std::expected<std::optional<Symbol>, std::error_code>
  loadSymbol(SymbolNode node, std::string_view usr, SymbolFacts facts);

  std::expected<void, std::error_code>
  replaceSymbolRow(SymbolId id, SymbolNode node, std::string_view identity,
                   const Symbol &symbol);
  std::expected<Symbol, std::error_code> loadSymbolRow(SymbolNode node,
                                                       SymbolId id);
  std::expected<Symbol, std::error_code> loadFacts(Symbol symbol,
                                                   SymbolFacts facts);

  std::expected<void, std::error_code>
  replaceDefinition(SymbolId id, FileId file,
                    const std::optional<Region> &definition);
  std::expected<std::optional<DefinitionFacts>, std::error_code>
  loadDefinition(SymbolId id);
  std::expected<void, std::error_code>
  replaceParameters(SymbolId id, std::span<const Parameter> parameters);
  std::expected<std::vector<Parameter>, std::error_code>
  loadParameters(SymbolId id);
  std::expected<void, std::error_code>
  replaceVariableInitializer(SymbolId id,
                             const std::optional<Initializer> &initializer);
  std::expected<std::optional<Initializer>, std::error_code>
  loadVariableInitializer(SymbolId id);
  std::expected<void, std::error_code>
  replaceEnumerationDetails(SymbolId id, const Enumeration &enumeration);
  std::expected<Enumeration, std::error_code>
  loadEnumerationDetails(Enumeration enumeration);
  std::expected<void, std::error_code>
  replaceEnumeratorDetails(SymbolId id, const Enumerator &enumerator);
  std::expected<Enumerator, std::error_code>
  loadEnumeratorDetails(Enumerator enumerator);
  std::expected<void, std::error_code>
  addTemplateParameters(SymbolId id,
                        std::span<const TemplateParameter> parameters);

  template <typename Model>
  std::expected<SymbolId, std::error_code>
  saveModel(SymbolNode node, const Model &model, SymbolFacts facts) {
    return saveSymbol(node, model, facts);
  }

  template <typename Model>
  std::expected<Model, std::error_code> loadModel(SymbolNode node, SymbolId id,
                                                  SymbolFacts facts) {
    return loadSymbol(node, id, facts).transform([](Symbol symbol) {
      return toModel<Model>(std::move(symbol));
    });
  }

  template <typename Model>
  std::expected<std::optional<Model>, std::error_code>
  loadModel(SymbolNode node, std::string_view usr, SymbolFacts facts) {
    return loadSymbol(node, usr, facts)
        .transform([](std::optional<Symbol> symbol) {
          if (!symbol) {
            return std::optional<Model>{};
          }
          return std::optional<Model>{toModel<Model>(std::move(*symbol))};
        });
  }

  template <typename Model>
  static Model toModel(Symbol symbol) {
    Model model;
    static_cast<Symbol &>(model) = std::move(symbol);
    return model;
  }

  std::unique_ptr<Connection> connection_;
};

} // namespace facts

#include "storage/Enumeration.h"
#include "storage/Function.h"
#include "storage/FunctionInstance.h"
#include "storage/FunctionTemplate.h"
#include "storage/Record.h"
#include "storage/RecordInstance.h"
#include "storage/RecordTemplate.h"
#include "storage/Symbol.h"
#include "storage/Variable.h"

#endif
