// model/Relation.h — an edge between two symbols.
//
// Everything a symbol points at is here rather than on the Symbol: its scope,
// its type, its bases, what it calls. Fields would force a decision about arity
// (one parent, but how many bases?) and would put a SymbolId on every symbol
// whether or not it has one; an edge list costs nothing for the symbols that
// have no edges.
//
// The kinds are the twenty already proven in cpp-indexer's `edge_kind` table,
// with the same names and the same numbering, so a row here is a row there. The
// direction is per kind and not always parent-to-child — `Contains` runs from
// the scope down, `MethodOf` runs from the member up — so each one says which
// way it points.

#ifndef FACTS_TOOL_MODEL_RELATION_H
#define FACTS_TOOL_MODEL_RELATION_H

#include "model/Symbol.h" // accessMask — the same low-two-bits layout
#include "model/SymbolId.h"

#include "clang/Basic/Specifiers.h"

#include <cstddef>
#include <cstdint>

namespace facts {

// Numbered from 1 to match cpp-indexer's edge_kind ids exactly.
enum class RelationKind : std::uint8_t {
  Calls = 1,      // source's body calls the function destination
  Inherits,       // source record derives from base destination
  Contains,       // source scope (namespace, class, function) declares dest
  Specializes,    // source is an explicit or partial specialization of the
                  // template destination
  Instantiates,   // source is an implicit instantiation of the template dest
  Overrides,      // source method overrides the virtual method destination
  Uses,           // source names destination without calling it — the catch-all
                  // reference edge, and the second most common one
  FieldOf,        // source data member belongs to record destination
  MethodOf,       // source method belongs to record destination
  ConstructValue, // the four ways destination gets built inside source: a named
  ConstructTemp,  // local, an unnamed temporary, `new`, and the copy and move
  ConstructHeap,  // constructors
  ConstructCopy,
  ConstructMove,
  FactoryConstruct, // source returns a fresh object it built — a factory
  Destroy,          // source destroys an object (explicit dtor call, delete)
  Friend,           // source record grants destination access to its privates
  DispatchCalls,    // a virtual call in source that reaches override dest — the
                    // resolved half of a Calls edge, kept apart because it is
                    // inferred, not written
  AliasOf,          // source alias resolves to destination
  OfType,           // source's declared type is the record destination

  // The three type_edge_kind names that join two declarations rather than two
  // layers of one type. The rest of a type's shape — pointer, reference, const
  // — is flags on the thing that uses it, not nodes in this graph.
  ReturnType = 21,     // source function returns destination
  ParamType,           // source function takes destination, at position
  TemplateArgumentType // source instance was given destination, at position
};

// What an edge carries beyond its two ends is kind-specific — a base clause has
// an access and a virtualness, a call has neither — so it goes in packed bits
// the way Symbol::flags does, not in fields that would be dead on every other
// kind. Same layout as there: bits 0-1 are a clang::AccessSpecifier, read with
// accessMask, and one bit each above it; five bits fit in a byte that would
// otherwise be padding next to kind.
enum RelationBit : std::size_t {
  VirtualBaseBit = 2, // Inherits: `class D : virtual B`
  ImplicitEdgeBit = 3, // the edge is compiler-generated — a call to a
                       // destructor at end of scope, an implicit conversion
  LexicalBit = 4      // Contains: the scope the declaration was *written* in,
                      // which for an out-of-line method is the namespace, not
                      // the class the semantic edge points at
};

struct Relation {
  SymbolId source;
  SymbolId destination;
  RelationKind kind = RelationKind::Uses;

  // Base access in the low two bits, RelationBit flags above it. AS_none until
  // an Inherits edge sets it. Sixteen bits and not eight: five are already in
  // use, and the extra byte lands in padding that kind and count leave behind,
  // so Relation is 24 bytes either way.
  std::uint16_t flags = clang::AS_none;

  // How many places this edge happens — one row per (source, destination,
  // kind), not one per occurrence. The occurrences themselves are a side PO
  // keyed by the edge; this is the answer to "how often" without reading them.
  //
  // Sixteen bits: this counts the calls to one callee from inside one caller,
  // not calls in the whole program. The busiest edge in the cpp-indexer index
  // is 332.
  std::uint16_t count = 1;

  // Where this edge sits when it is one of an ordered set — the n-th parameter
  // of a function type, the n-th template argument, the n-th base. Zero when
  // the kind is not ordered, which is most of them.
  std::uint16_t position = 0;
};

} // namespace facts

#endif // FACTS_TOOL_MODEL_RELATION_H
