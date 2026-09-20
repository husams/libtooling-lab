#include "apis/index/Internal.h"
#include "storage/SemanticProperties.h"
#include <clang/Index/IndexSymbol.h>

namespace facts::apis::index {
Result<std::string> kindName(std::int64_t stored) {
  using Kind = clang::index::SymbolKind;
  if (stored < 0 || stored > storage::storedSymbolKind(Kind::Concept))
    return std::unexpected("invalid stored symbol kind");
#if CLANG_VERSION_MAJOR < 22
  if (stored == storage::firstShiftedKind) return std::string("include-directive");
#endif
  const auto kind = storage::symbolKindFromStored(stored);
  if (kind == Kind::InstanceMethod || kind == Kind::ClassMethod ||
      kind == Kind::StaticMethod) return std::string("method");
  if (kind == Kind::EnumConstant) return std::string("enumerator");
  return clang::index::getSymbolKindString(kind).str();
}
}
