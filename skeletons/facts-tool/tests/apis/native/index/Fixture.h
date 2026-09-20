#pragma once
#include "apis/index/Index.h"
#include "storage/catalog/Database.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <filesystem>

namespace index_test {
namespace fs = std::filesystem;
namespace index = facts::apis::index;
struct Fixture {
  fs::path root, project, first, second;
  Fixture();
  ~Fixture() { fs::remove_all(root); }
};
void execute(const fs::path &, const std::string &);
void verifySnapshot(Fixture &fixture);
void verifyFailures(Fixture &fixture);
void verifyLongCursor();
void verifyMigration();
void verifyObsoleteSources();
}
