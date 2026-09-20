#pragma once
#include "apis/watch/ignore/Ignore.h"
#include "config/GitHandles.h"
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iostream>

namespace ignore_test {
namespace fs = std::filesystem;
using facts::apis::Settings;
using facts::apis::watch::Ignore;
struct Fixture {
  fs::path root;
  Fixture() {
    auto pattern = (fs::temp_directory_path() / "facts-ignore-XXXXXX").string();
    const auto *created = mkdtemp(pattern.data());
    assert(created);
    root = fs::canonical(created);
  }
  ~Fixture() { fs::remove_all(root); }
  fs::path write(const fs::path &name, const std::string &text = "") {
    const auto path = root / name;
    fs::create_directories(path.parent_path());
    std::ofstream(path) << text;
    return path;
  }
  Ignore filter(const Settings &settings = {}) const {
    auto result = Ignore::create(root, settings);
    if (!result) std::cerr << result.error() << '\n';
    assert(result);
    return std::move(*result);
  }
  bool excluded(const Ignore &ignore, const fs::path &name, bool dir = false) {
    const auto result = ignore.excludes(root / name, dir);
    if (!result) std::cerr << result.error() << '\n';
    assert(result);
    return *result;
  }
};
void plainRules();
void yamlRules();
void trackedRules();
}
