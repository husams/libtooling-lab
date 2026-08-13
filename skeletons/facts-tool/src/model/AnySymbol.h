#ifndef FACTS_TOOL_MODEL_ANY_SYMBOL_H
#define FACTS_TOOL_MODEL_ANY_SYMBOL_H

#include "model/Enumeration.h"
#include "model/Function.h"
#include "model/Record.h"
#include "model/Symbol.h"
#include "model/TypeAlias.h"
#include "model/Variable.h"

#include <utility>
#include <variant>

namespace facts {

using AnySymbol =
    std::variant<Symbol, Function, Record, Enumeration, Variable, TypeAlias>;

template <typename Model>
Model toSymbolModel(Symbol symbol) {
  Model model;
  static_cast<Symbol &>(model) = std::move(symbol);
  return model;
}

inline AnySymbol classifySymbol(Symbol symbol) {
  switch (symbol.Kind) {
  case clang::index::SymbolKind::Function:
  case clang::index::SymbolKind::InstanceMethod:
  case clang::index::SymbolKind::ClassMethod:
  case clang::index::SymbolKind::StaticMethod:
  case clang::index::SymbolKind::Constructor:
  case clang::index::SymbolKind::Destructor:
  case clang::index::SymbolKind::ConversionFunction:
    return toSymbolModel<Function>(std::move(symbol));
  case clang::index::SymbolKind::Struct:
  case clang::index::SymbolKind::Class:
  case clang::index::SymbolKind::Protocol:
  case clang::index::SymbolKind::Extension:
  case clang::index::SymbolKind::Union:
    return toSymbolModel<Record>(std::move(symbol));
  case clang::index::SymbolKind::Enum:
    return toSymbolModel<Enumeration>(std::move(symbol));
  case clang::index::SymbolKind::Variable:
  case clang::index::SymbolKind::Field:
  case clang::index::SymbolKind::EnumConstant:
  case clang::index::SymbolKind::InstanceProperty:
  case clang::index::SymbolKind::ClassProperty:
  case clang::index::SymbolKind::StaticProperty:
  case clang::index::SymbolKind::Parameter:
    return toSymbolModel<Variable>(std::move(symbol));
  case clang::index::SymbolKind::TypeAlias:
    return toSymbolModel<TypeAlias>(std::move(symbol));
  default:
    return symbol;
  }
}

} // namespace facts

#endif
