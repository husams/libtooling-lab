#pragma once

#include "cli/Verbose.h"
#include "model/Relation.h"
#include "model/RelationSite.h"
#include "model/Symbol.h"

#include <clang/Index/IndexSymbol.h>

#include <expected>
#include <span>
#include <string_view>
#include <system_error>

namespace facts::cli {

inline std::string_view relationKindName(RelationKind kind) {
  switch (kind) {
  case RelationKind::Calls:
    return "calls";
  case RelationKind::Inherits:
    return "inherits";
  case RelationKind::Contains:
    return "contains";
  case RelationKind::Specializes:
    return "specializes";
  case RelationKind::Instantiates:
    return "instantiates";
  case RelationKind::Overrides:
    return "overrides";
  case RelationKind::Uses:
    return "uses";
  case RelationKind::FieldOf:
    return "field_of";
  case RelationKind::MethodOf:
    return "method_of";
  case RelationKind::ConstructValue:
    return "construct_value";
  case RelationKind::ConstructTemp:
    return "construct_temp";
  case RelationKind::ConstructHeap:
    return "construct_heap";
  case RelationKind::ConstructCopy:
    return "construct_copy";
  case RelationKind::ConstructMove:
    return "construct_move";
  case RelationKind::FactoryConstruct:
    return "factory_construct";
  case RelationKind::Destroy:
    return "destroy";
  case RelationKind::Friend:
    return "friend";
  case RelationKind::DispatchCalls:
    return "dispatch_calls";
  case RelationKind::AliasOf:
    return "alias_of";
  case RelationKind::OfType:
    return "of_type";
  case RelationKind::ReturnType:
    return "return_type";
  case RelationKind::ParamType:
    return "param_type";
  case RelationKind::TemplateArgumentType:
    return "template_argument_type";
  }
  return "unknown";
}

inline void traceSymbol(int verbosity, const Symbol &symbol, SymbolId id) {
  logVerbose(verbosity, 3,
             "facts-tool: trace: symbol persisted kind='{}' name='{}' usr='{}' "
             "file_id={} symbol_index={} line={} column={} flags={}",
             clang::index::getSymbolKindString(symbol.Kind).str(),
             symbol.qualifiedName, symbol.usr, id.file, id.index,
             symbol.loc.line, symbol.loc.column, symbol.flags);
}

inline void traceRelations(int verbosity, std::string_view operation,
                           std::span<const Relation> relations) {
  logVerbose(verbosity, 3,
             "facts-tool: trace: relation batch operation='{}' count={}",
             operation, relations.size());
  for (const auto &relation : relations) {
    logVerbose(
        verbosity, 3,
        "facts-tool: trace: relation kind='{}' source={}:{} destination={}:{} "
        "count={} position={} flags={}",
        relationKindName(relation.kind), relation.source.file,
        relation.source.index, relation.destination.file,
        relation.destination.index, relation.count, relation.position,
        relation.flags);
  }
}

inline void traceRelationSites(int verbosity,
                               std::span<const RelationSite> sites) {
  logVerbose(verbosity, 3, "facts-tool: trace: relation-site batch count={}",
             sites.size());
  for (const auto &site : sites) {
    logVerbose(verbosity, 3,
               "facts-tool: trace: relation-site kind='{}' source={}:{} "
               "destination={}:{} file_id={} line={} column={} position={}",
               relationKindName(site.kind), site.source.file, site.source.index,
               site.destination.file, site.destination.index, site.file,
               site.location.line, site.location.column, site.position);
  }
}

inline void
tracePersistenceResult(int verbosity, std::string_view operation,
                       std::expected<void, std::error_code> result) {
  if (result) {
    logVerbose(verbosity, 3,
               "facts-tool: trace: persistence operation='{}' result=success",
               operation);
    return;
  }
  logVerbose(verbosity, 3,
             "facts-tool: trace: persistence operation='{}' result=failure "
             "error='{}'",
             operation, result.error().message());
}

} // namespace facts::cli
