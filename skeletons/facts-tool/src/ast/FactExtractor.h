// FactExtractor.h — the AST side of the tool.
//
// One FrontendAction per translation unit; each one gets the whole ASTContext
// after parsing finishes and walks it, handing whatever it finds to the
// FactStore. The walk itself is empty for now — this file only owns the wiring
// that gets a live ASTContext and a store into the same place.

#ifndef FACTS_TOOL_AST_FACTEXTRACTOR_H
#define FACTS_TOOL_AST_FACTEXTRACTOR_H

#include <memory>

namespace clang::tooling {
class FrontendActionFactory;
} // namespace clang::tooling

namespace facts {

class FactStore;
class FileManager;

// The factory outlives every action it creates; the store must outlive the
// factory, so main owns it.
std::unique_ptr<clang::tooling::FrontendActionFactory>
createFactExtractorFactory(FileManager &files, FactStore &store);

} // namespace facts

#endif // FACTS_TOOL_AST_FACTEXTRACTOR_H
