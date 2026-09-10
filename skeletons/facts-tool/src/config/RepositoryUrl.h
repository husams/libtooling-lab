#pragma once

#include <string>
#include <string_view>

namespace facts::config {

// The last non-empty path segment of a remote URL, with a trailing "/"
// stripped first and a trailing ".git" suffix stripped after. A segment is
// split on both '/' and ':' so scp-style remotes ("git@host:owner/repo.git")
// work the same as URL-style ones. Returns an empty string when nothing
// usable remains (an empty URL, or one that reduces to "" / "." / "..").
// Exposed separately from repositoryName() so it can be unit-tested without
// a real repository.
std::string repositoryNameFromUrl(std::string_view url);

} // namespace facts::config
