#include "commands/catalog/Display.h"
#include <format>

namespace facts::commands {

catalog::Result<std::string>
displayComponents(const std::vector<catalog::Component> &values) {
  std::string output = "ID\tNAME\tKIND\tVERSION\tREPOSITORY\tFILES\tROOT\n";
  for (const auto &value : values) {
    auto root = catalog::componentRoot(value);
    if (!root)
      return std::unexpected(root.error());
    output += std::format("{}\t{}\t{}\t{}\t{}\t{}\t{}\n", value.value.id,
                          value.value.name, value.value.kind,
                          value.value.version.value_or("-"), value.repository,
                          value.files, root->string());
  }
  return output;
}

std::string
displayRepositories(const std::vector<catalog::Repository> &values) {
  std::string output = "ID\tNAME\tKIND\tCOMPONENTS\tCLONES\tACTIVE CLONE\n";
  for (const auto &value : values) {
    output += std::format("{}\t{}\t{}\t{}\t{}\t{}\n", value.id, value.name,
                          value.kind, value.components, value.clones,
                          value.activePath);
  }
  return output;
}

std::string displayDirectories(const std::vector<catalog::Directory> &values) {
  std::string output = "ID\tCOMPONENT\tFILES\tPATH\n";
  for (const auto &value : values) {
    output += std::format("{}\t{}\t{}\t{}\n", value.id, value.component,
                          value.files, value.path);
  }
  return output;
}
} // namespace facts::commands
