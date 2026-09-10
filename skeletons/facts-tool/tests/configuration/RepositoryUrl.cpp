#include "Sandbox.h"
#include "config/RepositoryUrl.h"

namespace configuration_test {

void repositoryUrl() {
  using facts::config::repositoryNameFromUrl;
  // Every documented URL shape.
  assert(repositoryNameFromUrl("https://host/owner/repo.git") == "repo");
  assert(repositoryNameFromUrl("ssh://git@host/owner/repo") == "repo");
  assert(repositoryNameFromUrl("git@host:owner/repo.git") == "repo");
  assert(repositoryNameFromUrl("git@host:repo") == "repo");
  assert(repositoryNameFromUrl("/path/to/repo.git/") == "repo");
  assert(repositoryNameFromUrl("file:///path/repo") == "repo");
  // Edge cases: nothing usable, and trailing-slash variants.
  assert(repositoryNameFromUrl("") == "");
  assert(repositoryNameFromUrl("/") == "");
  assert(repositoryNameFromUrl("https://host/") == "host");
  assert(repositoryNameFromUrl("https://host/owner/repo.git//") == "repo");
  assert(repositoryNameFromUrl("https://host/owner/..") == "");
}
} // namespace configuration_test
