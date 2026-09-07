#include "ast/visitors/CallGraphVisitor.h"

#include "ast/StoreExtracted.h"
#include "ast/extractors/DestructorCalls.h"

#include <clang/AST/Decl.h>

#include <iterator>

namespace facts {

IndexingResult
CallGraphVisitor::collectDestructors(const clang::FunctionDecl &caller,
                                     callgraph::CallGraphFacts &facts) {
  const auto *definition = caller.getDefinition();
  if (!definition)
    return {};
  return extractDestructorCalls(*definition, context_, files_, store_)
      .transform([&](auto calls) {
        facts.calls.insert(facts.calls.end(),
                           std::make_move_iterator(calls.begin()),
                           std::make_move_iterator(calls.end()));
      })
      .transform_error([](ExtractionError error) {
        return IndexingError{"cannot extract destructor invocation: " +
                             std::string{extractionErrorName(error)}};
      });
}

} // namespace facts
