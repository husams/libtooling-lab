#include "apis/watch/catalog/Exclusions.h"

namespace facts::apis::watch {
namespace {
bool samePath(const std::string &selector, const std::filesystem::path &path) {
  const std::filesystem::path candidate(selector);
  if (!candidate.is_absolute()) return false;
  std::error_code error;
  const auto canonical = std::filesystem::weakly_canonical(candidate, error);
  return (error ? candidate.lexically_normal() : canonical) == path;
}
bool selectedClone(const Clone &clone, const std::string &selector) {
  return selector == std::to_string(clone.cloneId) ||
         (!clone.label.empty() && (selector == clone.label ||
                                   selector == clone.repository + ":" + clone.label)) ||
         samePath(selector, clone.path);
}
}
std::string exclusion(const Clone &clone, const Settings &settings) {
  for (const auto &selector : settings.excludedRepositories)
    if (selector == clone.repository || selector == std::to_string(clone.repositoryId))
      return "repository excluded by " + selector;
  for (const auto &selector : settings.excludedClones)
    if (selectedClone(clone, selector)) return "clone excluded by " + selector;
  return {};
}
}
