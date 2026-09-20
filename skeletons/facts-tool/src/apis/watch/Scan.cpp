#include "apis/watch/Scan.h"
#include "apis/watch/Paths.h"

namespace facts::apis::watch {
std::expected<Scan, std::string> discover(const Settings &settings,
                                         const std::atomic_bool &cancelled) {
  Scan result;
  for (const auto &configured : settings.directories) {
    const auto root = watch::absolute(configured, settings);
    std::error_code error;
    if (!std::filesystem::is_directory(root, error))
      return std::unexpected("watch directory is unavailable: " + root.string());
    result.directories.insert(root);
    auto iterator = std::filesystem::recursive_directory_iterator(root, error);
    const auto end = std::filesystem::recursive_directory_iterator{};
    while (!error && iterator != end) {
      if (cancelled) return std::unexpected("watch scan cancelled");
      const auto &entry = *iterator;
      if (watch::ignored(entry.path(), settings)) {
        iterator.disable_recursion_pending();
      } else if (entry.is_symlink(error)) {
        iterator.disable_recursion_pending();
        if (entry.path().filename() == "compile_commands.json")
          result.databases.insert(entry.path().parent_path());
      } else if (entry.is_directory(error)) {
        result.directories.insert(entry.path());
      } else if (entry.path().filename() == "compile_commands.json") {
        result.databases.insert(entry.path().parent_path());
      }
      iterator.increment(error);
    }
    if (error) return std::unexpected("cannot scan " + root.string() + ": " +
                                      error.message());
  }
  return result;
}
}
