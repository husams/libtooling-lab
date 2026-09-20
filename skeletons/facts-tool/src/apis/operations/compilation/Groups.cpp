#include "apis/operations/compilation/Candidates.h"
#include <algorithm>
#include <map>

namespace facts::apis::operations::compilation {
std::vector<Group> groupCandidates(std::vector<Candidate> candidates,
    const std::filesystem::path &header, const std::set<FileId> &known) {
  using Key = std::pair<std::string, std::vector<std::string>>;
  std::map<Key, Group> groups;
  for (auto &candidate : candidates) {
    auto transferred = clang::tooling::transferCompileCommand(candidate.command, header.string());
    Key key{transferred.Directory, transferred.CommandLine};
    auto [group, inserted] = groups.try_emplace(std::move(key), Group{std::move(transferred), {}});
    group->second.sources.push_back(std::move(candidate));
  }
  std::vector<Group> result;
  for (auto &[key, group] : groups) {
    std::ranges::stable_sort(group.sources, [&](const auto &left, const auto &right) {
      return known.contains(left.id) > known.contains(right.id);
    });
    result.push_back(std::move(group));
  }
  return result;
}
}
