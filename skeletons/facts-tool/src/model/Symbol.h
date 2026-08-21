// model/Symbol.h — one named entity, copied out of the AST.
//
// Plain data only: no Decl pointers, no SourceLocation, no behaviour. A Symbol
// stays valid long after the translation unit it came from is gone.

#ifndef FACTS_TOOL_MODEL_SYMBOL_H
#define FACTS_TOOL_MODEL_SYMBOL_H

#include "model/Location.h"
#include "model/Parameter.h"
#include "model/SymbolId.h"

#include "clang/Basic/Specifiers.h"
#include "clang/Index/IndexSymbol.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace facts {

// Three of the fields in Symbol::flags are not single bits: bits 0-1 hold a
// clang::AccessSpecifier (AS_public 0 .. AS_none 3), bits 9-10 hold a
// clang::RefQualifierKind (RQ_None / RQ_LValue / RQ_RValue), and bits 22-23
// hold a clang::ConstexprSpecKind (Unspecified / Constexpr / Consteval /
// Constinit). Read them with the masks; everything else is one bit per yes/no
// fact that would otherwise cost a whole byte as a bool.
//
//   access: clang::AccessSpecifier(flags & accessMask)
//   ref-q:  clang::RefQualifierKind((flags >> refQualifierShift)
//                                   & refQualifierMask)
//   cxpr:   clang::ConstexprSpecKind((flags >> constexprShift) & constexprMask)
//   a flag: flags & bit(PureBit) / flags |= bit(VirtualBit)
//
// A plain uint32_t and not a std::bitset: libc++ stores every bitset up to 64
// bits in one size_t, so even bitset<8> costs 8 bytes. 25 bits are in use.
//
// Template-ness and function-local-ness are not here — SymbolInfo::Properties
// already carries Generic / TemplateSpecialization / Local.
inline constexpr std::uint32_t accessMask = 0b0000'0011;
inline constexpr std::uint32_t refQualifierShift = 9;
inline constexpr std::uint32_t refQualifierMask = 0b0000'0011;
inline constexpr std::uint32_t constexprShift = 22;
inline constexpr std::uint32_t constexprMask = 0b0000'0011;

// The bit at a position, for setting and testing: flags |= bit(StaticBit).
inline constexpr std::uint32_t bit(std::size_t position) {
  return std::uint32_t{1} << position;
}

enum SymbolBit : std::size_t {
  DefinitionBit = 2, // has a body / initializer, not just a declaration
  ImplicitBit = 3,   // compiler-generated, never written in the source
  StaticBit = 4,
  VirtualBit = 5,
  ConstBit = 6, // const-qualified method or top-level const value
  InlineBit = 7,
  PureBit = 8, // pure virtual — `= 0`, FunctionDecl::isPureVirtual()
  // bits 9-10: ref-qualifier field, see above
  OverrideBit = 11, // overrides a base method — size_overridden_methods() != 0,
                    // which is true with or without the `override` keyword
  InternalLinkageBit = 12, // static, or in an anonymous namespace: two TUs can
                           // hold this USR and mean different entities
  ExternalBit = 13, // declared outside the sources being analyzed (a system or
                    // third-party header) — nothing here will ever define it

  // Kind-specific, but still one bit each, so they live here with the rest:
  // ConstBit and PureBit above are already function-only facts.
  VariadicBit = 14,  // C varargs — `printf(const char *, ...)`
  DeletedBit = 15,   // `= delete`
  DefaultedBit = 16, // `= default`
  ExplicitBit = 17,  // explicit constructor or conversion operator
  FinalBit = 18,
  AbstractBit = 19,      // a record with an unimplemented pure virtual
  PolymorphicBit = 20,   // a record with a vtable
  ExternStorageBit = 21, // the declaration was written with extern storage
  // bits 22-23: constexpr/consteval/constinit field, see above
  NoexceptBit = 24, // declared not to throw. The written spec has fourteen
                    // spellings (EST_BasicNoexcept, EST_NoexceptTrue,
                    // EST_DynamicNone, ...) and one useful question.
};

// Kind / SubKind / Lang / Properties come from the base — one
// index::getSymbolInfo(D) fills it. SymbolInfo has no default member
// initializers, so build a Symbol with {} (Symbol s{}) to zero the base.
struct Symbol : clang::index::SymbolInfo {
  // Unique across the run: file half + per-file index. How other facts point
  // at this symbol, and where it came from, in 8 bytes.
  SymbolId id;

  std::string usr; // identity: stable across TUs and parses
  // Everything a lookup needs — scope, name, and for functions the signature:
  // "geo::Circle::area() const". A short-name search is '%area%' over this.
  std::string qualifiedName;

  // The path lives once in the file table; id.file already says which file
  // this was declared in, so only the position is stored here.
  Location loc;

  // The token-inclusive source range of this declaration when it defines the
  // symbol. Declarations without a definition leave this empty.
  std::optional<Region> definition;
  // A merged symbol's stable id may belong to a declaration in another file.
  // Keep the definition's actual file alongside its range when known.
  std::optional<FileId> definitionFile;

  // Parameters in source order. The vector position is the parameter index.
  std::vector<Parameter> parameters;

  // Access in the low two bits, SymbolBit flags above it. Defaults to
  // AS_none — not a class member, nothing else known.
  std::uint32_t flags = clang::AS_none;
};

} // namespace facts

#endif // FACTS_TOOL_MODEL_SYMBOL_H
