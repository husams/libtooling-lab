#pragma once

#include "tooling/astcache/Options.h"

#include <string>
#include <vector>

namespace clang::tooling {
class CompilationDatabase;
}

namespace facts {
class FileManager;
class FactStore;
class IndexingStatus;
}

namespace facts::commands {
int extractTranslationUnits(const clang::tooling::CompilationDatabase &database,
                            const std::vector<std::string> &sources,
                            FileManager &files, FactStore &store,
                            IndexingStatus &status,
                            const astcache::Options &cache);
}
