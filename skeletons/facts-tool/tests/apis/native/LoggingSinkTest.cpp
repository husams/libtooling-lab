#include "apis/logging/Logger.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sys/stat.h>

using namespace facts::apis::logging;
namespace fs = std::filesystem;
int main() {
  auto pattern = (fs::temp_directory_path() / "facts-log-sink-XXXXXX").string();
  const char *created = ::mkdtemp(pattern.data());
  assert(created);
  const fs::path root = created;
  const auto pipe = root / "blocked.log";
  assert(::mkfifo(pipe.c_str(), 0600) == 0);
  for (const auto &path : {root, pipe}) {
    bool rejected = false;
    try { Logger logger(Options{path, Level::off}); }
    catch (const std::runtime_error &) { rejected = true; }
    assert(rejected);
  }
  const auto file = root / "events.jsonl";
  {
    Logger logger(Options{file, Level::off});
    logger.write(Level::error, "must.not.be.recorded");
  }
  assert(fs::is_regular_file(file) && fs::file_size(file) == 0);
  {
    Logger logger(Options{file, Level::info});
    logger.write(Level::info, "first.run");
    logger.write(Level::debug, "must.not.be.recorded");
  }
  {
    Logger logger(Options{file, Level::info});
    logger.write(Level::error, "second.run", {{"large", std::string(1000000, 'x')}});
  }
  std::ifstream input(file);
  std::string first, second, extra;
  assert(std::getline(input, first) && std::getline(input, second));
  assert(!std::getline(input, extra));
  assert(nlohmann::json::parse(first).at("event") == "first.run");
  const auto oversized = nlohmann::json::parse(second);
  assert(oversized.at("event") == "second.run");
  assert(oversized.at("fields").at("truncated") == true);
  assert(second.size() < 16384);
  fs::remove_all(root);
  std::cout << "off validation, empty sink, filtering, append and bounded records PASS\n";
}
