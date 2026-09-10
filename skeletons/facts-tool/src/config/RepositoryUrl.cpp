#include "config/RepositoryUrl.h"

namespace facts::config {

std::string repositoryNameFromUrl(std::string_view url) {
  std::string text(url);
  while (!text.empty() && text.back() == '/') text.pop_back();
  if (text.empty()) return {};
  std::size_t start = 0;
  if (const auto slash = text.find_last_of('/'); slash != std::string::npos) start = slash + 1;
  if (const auto colon = text.find_last_of(':'); colon != std::string::npos && colon + 1 > start)
    start = colon + 1;
  std::string name = text.substr(start);
  if (name.size() > 4 && name.ends_with(".git")) name.resize(name.size() - 4);
  if (name.empty() || name == "." || name == "..") return {};
  return name;
}

} // namespace facts::config
