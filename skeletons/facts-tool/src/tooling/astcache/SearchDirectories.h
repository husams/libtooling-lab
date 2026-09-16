#pragma once

#include <filesystem>
#include <vector>

namespace clang {
class Preprocessor;
}

namespace facts::astcache::detail {
std::vector<std::filesystem::path>
headerSearchDirectories(clang::Preprocessor &preprocessor,
                        const std::filesystem::path &workingDirectory);
} // namespace facts::astcache::detail
