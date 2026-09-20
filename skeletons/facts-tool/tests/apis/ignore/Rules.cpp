#include "Fixture.h"

namespace ignore_test {
void plainRules() {
  Fixture fixture;
  fixture.write(".gitignore", "*.hpp\n/generated/\n!generated/keep.cpp\n");
  fixture.write("sub/.gitignore", "!keep.hpp\n/root.cpp\n**/secret*.cpp\n");
  fixture.write("sub/keep.hpp");
  fixture.write("sub/other.hpp");
  fixture.write("sub/root.cpp");
  fixture.write("sub/deep/root.cpp");
  fixture.write("generated/keep.cpp");
  fixture.write(".cache/allowed.cpp");
  const auto ignore = fixture.filter();
  assert(!fixture.excluded(ignore, ".", true));
  assert(fixture.excluded(ignore, "sub/other.hpp"));
  assert(!fixture.excluded(ignore, "sub/keep.hpp"));
  assert(fixture.excluded(ignore, "sub/root.cpp"));
  assert(!fixture.excluded(ignore, "sub/deep/root.cpp"));
  assert(fixture.excluded(ignore, "sub/deep/secret-data.cpp"));
  assert(fixture.excluded(ignore, "generated", true));
  assert(fixture.excluded(ignore, "generated/keep.cpp"));
  assert(!fixture.excluded(ignore, ".cache/allowed.cpp"));
  assert(fixture.excluded(ignore, ".git", true));
  assert(!ignore.excludes(fixture.root.parent_path() / "outside.cpp", false));
  assert(ignore.controlFiles().empty());
  assert(!fs::exists(fixture.root / ".git"));
  const auto trailingRoot = Ignore::create(fixture.root / "", {});
  assert(trailingRoot);
  assert(!fixture.excluded(*trailingRoot, "sub/keep.hpp"));
  assert(!Ignore::create("relative", {}));
  fixture.write(".gitignore", "new.cpp\n");
  const auto refreshed = fixture.filter();
  assert(fixture.excluded(refreshed, "new.cpp"));
  assert(!fixture.excluded(refreshed, "old.hpp"));
}

void yamlRules() {
  Fixture fixture;
  fixture.write(".gitignore", "*.hpp\n");
  fixture.write("sub/.gitignore", "!keep.hpp\n");
  Settings settings;
  settings.excludePatterns = {"*.cpp", "!keep.cpp", "/build/**", "**/hidden/"};
  settings.excludedDirectories = {"vendor", (fixture.root / "absolute").string()};
  auto ignore = fixture.filter(settings);
  assert(fixture.excluded(ignore, "source.cpp"));
  assert(!fixture.excluded(ignore, "keep.cpp"));
  assert(fixture.excluded(ignore, "build/source.hpp"));
  assert(fixture.excluded(ignore, "sub/hidden/keep.cpp"));
  assert(fixture.excluded(ignore, "vendor", true));
  assert(fixture.excluded(ignore, "vendor/keep.cpp"));
  assert(fixture.excluded(ignore, "absolute/keep.cpp"));
  assert(!fixture.excluded(ignore, "vendor-other/keep.cpp"));
  assert(!fixture.excluded(ignore, "sub/keep.hpp"));
  settings.excludedDirectories = {"."};
  assert(fixture.excluded(fixture.filter(settings), ".", true));
}
}
