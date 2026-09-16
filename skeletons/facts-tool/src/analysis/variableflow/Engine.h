#pragma once

#include "analysis/variableflow/Model.h"
#include "tooling/astcache/Options.h"

#include <clang/Tooling/CompilationDatabase.h>

#include <expected>
#include <string>
#include <vector>

namespace facts::variableflow {

std::expected<Graph, std::string>
analyse(clang::tooling::CompilationDatabase &database,
        const std::vector<std::string> &sources, const Request &request,
        const astcache::Options &astCache = {});

} // namespace facts::variableflow
