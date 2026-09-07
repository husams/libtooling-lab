#ifndef FACTS_TOOL_STORAGE_FACT_STORE_H
#define FACTS_TOOL_STORAGE_FACT_STORE_H

#include "analysis/callgraph/CallGraphTypes.h"
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
#include <vector>

namespace facts {

class FactStore {
public:
  explicit FactStore(std::string path, int verbosity = 0);

  std::expected<void, std::error_code> begin();
  std::expected<void, std::error_code> end(bool reportSummary = true);
  std::expected<void, std::error_code> rollback();

  std::expected<void, std::error_code>
  registerFactProvenance(std::span<const storage::FactProvenance> rows,
                         std::span<const FileId> selected = {}) {
    return storage_.registerFactProvenance(rows, selected);
  }

  std::expected<void, std::error_code>
  upsertMatchedSymbols(std::span<const MatchedSymbol> symbols) {
    return storage_.upsertMatchedSymbols(symbols);
  }

  std::expected<void, std::error_code>
  addExpressionOccurrences(std::span<const ExpressionOccurrence> occurrences) {
    return storage_.addExpressionOccurrences(occurrences);
  }

  std::expected<void, std::error_code>
  addSourceRegions(std::span<const SourceRegion> regions) {
    return storage_.addSourceRegions(regions);
  }

  std::expected<void, std::error_code>
  addCallGraphEntries(std::span<const CallGraphEntry> entries) {
    return storage_.addCallGraphEntries(entries);
  }

  std::expected<void, std::error_code>
  invalidateCallGraphEntries(std::span<const SymbolId> symbols) {
    return storage_.invalidateCallGraphEntries(symbols);
  }

  std::expected<void, std::error_code>
  invalidateCallGraphEntries(std::span<const FileId> files) {
    return storage_.invalidateCallGraphEntries(files);
  }

  std::expected<void, std::error_code> invalidateCallGraphEntries(FileId file) {
    return storage_.invalidateCallGraphEntries(file);
  }

  std::expected<std::optional<CallGraphEntry>, std::error_code>
  findCallGraphEntry(SymbolId symbol) {
    return storage_.findCallGraphEntry(symbol);
  }

  std::expected<bool, std::error_code> isExternal(SymbolId symbol) {
    return storage_.isExternal(symbol);
  }

  std::expected<void, std::error_code> addCallGraphExternalReferences(
      std::span<const ExternalReference> references) {
    return storage_.addCallGraphExternalReferences(references);
  }

  std::expected<void, std::error_code>
  addCallGraphFacts(std::span<const Relation> relations,
                    std::span<const RelationSite> sites,
                    std::span<const ExternalReference> references,
                    std::span<const CallGraphEntry> entries,
                    std::span<const UnresolvedCallSite> unresolved = {}) {
    return storage_.addCallGraphFacts(relations, sites, references, entries,
                                      unresolved);
  }

  std::expected<void, std::error_code>
  addUnresolvedCallSites(std::span<const UnresolvedCallSite> sites) {
    return storage_.addUnresolvedCallSites(sites);
  }

  std::expected<void, std::error_code>
  clearUnresolvedCallSites(std::span<const SymbolId> callers) {
    return storage_.clearUnresolvedCallSites(callers);
  }

  std::expected<void, std::error_code>
  clearCallGraphFacts(std::span<const SymbolId> callers) {
    return storage_.clearCallGraphFacts(callers);
  }

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

  std::expected<void, std::error_code> saveReturnType(SymbolId callable,
                                                      const ReturnType &type) {
    auto result = storage_.saveReturnType(callable, type);
    cli::tracePersistenceResult(verbosity_, "save-return-type", result);
    return result;
  }

  bool contains(std::string_view usr) const;

  std::size_t cachedIdCount() const { return idsByUsr_.size(); }

  unsigned count() const { return static_cast<unsigned>(idsByUsr_.size()); }

  int verbosity() const { return verbosity_; }

  void stageCallGraphEntry(SymbolId symbol) {
    callGraphEntries_.push_back(symbol);
  }

  void stageCallableInvocation(callgraph::CallFact fact) {
    callableInvocations_.push_back(std::move(fact));
  }

  std::vector<callgraph::CallFact> takeCallableInvocations() {
    return std::exchange(callableInvocations_, {});
  }

  void stageUnresolvedCallSite(UnresolvedCallSite site) {
    unresolvedCallSites_.push_back(std::move(site));
  }

  const std::vector<SymbolId> &callGraphEntries() const {
    return callGraphEntries_;
  }

  const std::vector<UnresolvedCallSite> &unresolvedCallSites() const {
    return unresolvedCallSites_;
  }

  std::vector<SymbolId> takeCallGraphEntries() {
    return std::exchange(callGraphEntries_, std::vector<SymbolId>{});
  }

  std::vector<UnresolvedCallSite> takeUnresolvedCallSites() {
    return std::exchange(unresolvedCallSites_,
                         std::vector<UnresolvedCallSite>{});
  }

private:
  void remember(std::string_view usr, SymbolId id);

  Storage storage_;
  std::unordered_map<std::string, SymbolId> idsByUsr_;
  std::vector<SymbolId> callGraphEntries_;
  std::vector<callgraph::CallFact> callableInvocations_;
  std::vector<UnresolvedCallSite> unresolvedCallSites_;
  int verbosity_ = 0;
};

} // namespace facts

#endif
