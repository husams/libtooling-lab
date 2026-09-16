#pragma once

#include <compare>
#include <string>
#include <vector>

namespace facts::astcache {

struct Input {
  std::string path;
  auto operator<=>(const Input &) const = default;
};

struct Include {
  std::string source;
  std::string target;
  auto operator<=>(const Include &) const = default;
};

struct Revision {
  std::string path;
  std::string commit;
  auto operator<=>(const Revision &) const = default;
};

struct Snapshot {
  std::string key;
  std::string source;
  std::string working_directory;
  std::string generation;
  std::vector<Input> inputs;
  std::vector<Include> includes;
  std::vector<Revision> revisions;
};

struct Artifact {
  std::string key;
  std::string path;
  std::string digest;
  std::string generation;
};

} // namespace facts::astcache
