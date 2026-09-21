#include "apis/watch/Paths.h"
#include <set>

namespace facts::apis::watch {
std::filesystem::path absolute(const std::filesystem::path &path,
                               const Settings &settings) {
  return (path.is_absolute() ? path : settings.workingDirectory / path)
      .lexically_normal();
}

bool ignored(const std::filesystem::path &path, const Settings &settings) {
  static const std::set<std::string> directories{".git"};
  for (const auto &part : path)
    if (directories.contains(part.string())) return true;
  const auto name = path.filename().string();
  const auto extension = path.extension().string();
  if (extension == ".db" || extension == ".sqlite" || extension == ".sqlite3" ||
      extension == ".ast" || extension == ".o" || extension == ".obj" ||
      extension == ".pcm" || extension == ".pch" || name.ends_with("-wal") ||
      name.ends_with("-shm") || name.ends_with("-journal")) return true;
  const auto config = absolute(settings.serverConfig, settings).string();
  const auto candidate = absolute(path, settings).string();
  if (!settings.logging.file.empty() && candidate == settings.logging.file.string()) return true;
  return !settings.serverConfig.empty() &&
         (candidate == config || candidate.starts_with(config + "."));
}

bool relevant(const std::filesystem::path &path) {
  static const std::set<std::string> extensions{
      ".c", ".C", ".cc", ".cp", ".cpp", ".cxx", ".c++", ".m", ".mm",
      ".h", ".H", ".hh", ".hpp", ".hxx", ".h++", ".inc", ".ipp",
      ".inl", ".def", ".icc", ".tcc", ".txx", ".i", ".ii", ".tpp",
      ".ixx", ".cppm", ".cu", ".cuh"};
  return path.filename() == "compile_commands.json" || path.filename() == ".facts-tool.yaml" ||
         path.extension().empty() ||
         extensions.contains(path.extension().string());
}
}
