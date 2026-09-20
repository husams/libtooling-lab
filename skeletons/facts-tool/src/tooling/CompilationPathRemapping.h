#pragma once

#include "tooling/StoredCompilationReader.h"

namespace facts {
std::string remapCompilePath(std::string value,
                             const CompilePathRemapping &remapping);
} // namespace facts
