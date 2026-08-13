// model/SymbolId.h — a symbol's identity, unique across the whole run.
//
// Two 32-bit halves: which file, and which symbol within that file. Numbering
// restarts per file, so one TU can be indexed without knowing about any other —
// and the pair is still globally unique. The file half also means a symbol
// already says where it came from, without carrying the path.

#ifndef FACTS_TOOL_MODEL_SYMBOLID_H
#define FACTS_TOOL_MODEL_SYMBOLID_H

#include <compare>
#include <cstdint>
#include <functional>

namespace facts {

using FileId = std::uint32_t;

inline constexpr FileId builtinFileId = 0;
inline constexpr FileId firstPhysicalFileId = 1;

struct SymbolId {
  FileId file = builtinFileId; // which file; zero is compiler-provided
  std::uint32_t index = 0;     // n-th symbol found in it

  constexpr std::uint64_t packed() const noexcept {
    return (static_cast<std::uint64_t>(file) << 32U) | index;
  }

  // Defaulted so an id can key a map or sort — the two halves compare in order,
  // which groups a file's symbols together.
  friend bool operator==(const SymbolId &, const SymbolId &) = default;
  friend std::strong_ordering operator<=>(const SymbolId &,
                                          const SymbolId &) = default;
};

} // namespace facts

namespace std {

template <>
struct hash<facts::SymbolId> {
  std::size_t operator()(const facts::SymbolId &id) const noexcept {
    return hash<std::uint64_t>{}(id.packed());
  }
};

} // namespace std

#endif // FACTS_TOOL_MODEL_SYMBOLID_H
