#include "Fixture.h"
#include <algorithm>

namespace ignore_test {
void trackedRules() {
  Fixture fixture;
  facts::config::detail::ensureLibgit2Initialized();
  facts::config::detail::RepositoryHandle repository;
  assert(git_repository_init(&repository.repo, fixture.root.c_str(), 0) == 0);
  fixture.write(".gitignore", "*.cpp\n/generated/\n");
  fixture.write("tracked.cpp");
  fixture.write("generated/tracked.cpp");
  facts::config::detail::IndexHandle index;
  assert(git_repository_index(&index.index, repository.repo) == 0);
  assert(git_index_add_bypath(index.index, "tracked.cpp") == 0);
  assert(git_index_add_bypath(index.index, "generated/tracked.cpp") == 0);
  assert(git_index_write(index.index) == 0);
  fixture.write(".git/info/exclude", "private.hpp\n");
  const auto ignore = fixture.filter();
  assert(!fixture.excluded(ignore, "tracked.cpp"));
  assert(fixture.excluded(ignore, "untracked.cpp"));
  assert(!fixture.excluded(ignore, "generated", true));
  assert(!fixture.excluded(ignore, "generated/tracked.cpp"));
  assert(fixture.excluded(ignore, "generated/untracked.cpp"));
  assert(fixture.excluded(ignore, "private.hpp"));
  const auto controls = ignore.controlFiles();
  assert(std::ranges::find(controls, fixture.root / ".git/index") != controls.end());
  assert(std::ranges::find(controls, fixture.root / ".git/info/exclude") != controls.end());
  Settings settings;
  settings.excludePatterns = {"*.cpp"};
  assert(fixture.excluded(fixture.filter(settings), "tracked.cpp"));
  settings.excludePatterns.clear();
  settings.excludedDirectories = {"generated"};
  const auto denied = fixture.filter(settings);
  assert(fixture.excluded(denied, "generated", true));
  assert(fixture.excluded(denied, "generated/tracked.cpp"));
  fixture.write("late.cpp");
  assert(git_index_add_bypath(index.index, "late.cpp") == 0);
  assert(git_index_write(index.index) == 0);
  assert(!fixture.excluded(fixture.filter(), "late.cpp"));
}
}
