#include "analysis/callgraph/CallGraphRequest.h"

namespace facts::callgraph {

std::string_view scopeName(CallsScope scope) {
  switch (scope) {
  case CallsScope::project:
    return "project";
  case CallsScope::library:
    return "library";
  case CallsScope::all:
    return "all";
  }
  return "all";
}

} // namespace facts::callgraph
