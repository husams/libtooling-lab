#include "apis/watch/Paths.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <filesystem>
#include <iostream>

int main() {
  facts::apis::Settings settings;
  settings.workingDirectory = std::filesystem::temp_directory_path() / "facts-watch-log";
  settings.serverConfig = settings.workingDirectory / "server.yaml";
  settings.logging.file = settings.workingDirectory / "clone" / "server-trace.cpp";
  using namespace facts::apis::watch;
  assert(relevant(settings.logging.file));
  assert(ignored(settings.logging.file, settings));
  assert(ignored("clone/server-trace.cpp", settings));
  assert(!ignored(settings.logging.file.parent_path() / "source.cpp", settings));
  std::cout << "custom logging destination is excluded from repository change detection PASS\n";
}
