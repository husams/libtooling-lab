#pragma once

#include <expected>
#include <string>

namespace facts::cli {
struct DependencyOptions;
}

namespace facts::commands {

std::expected<int, std::string>
runDependency(const cli::DependencyOptions &options);

} // namespace facts::commands
