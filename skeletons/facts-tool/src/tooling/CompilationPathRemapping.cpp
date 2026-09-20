#include "tooling/CompilationPathRemapping.h"

namespace facts {
std::string remapCompilePath(std::string value,
                             const CompilePathRemapping &remapping) {
  if (remapping.from.empty() || remapping.to.empty()) return value;
  const bool equals = value.starts_with('=');
  const auto path = std::filesystem::path(equals ? value.substr(1) : value);
  if (!path.is_absolute()) return value;
  const auto relative = path.lexically_normal().lexically_relative(
      remapping.from.lexically_normal());
  if (relative.empty() || relative.is_absolute() || *relative.begin() == "..")
    return value;
  if (relative == ".")
    return (equals ? "=" : "") + remapping.to.lexically_normal().string();
  return (equals ? "=" : "") +
         (remapping.to / relative).lexically_normal().string();
}
} // namespace facts
