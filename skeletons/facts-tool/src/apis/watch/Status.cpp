#include "apis/watch/State.h"

namespace facts::apis {
Json Watcher::Impl::status() const {
  Json directories = Json::array();
  for (const auto &directory : settings.directories)
    directories.push_back(watch::absolute(directory, settings).string());
  const bool stored = settings.importArguments.empty() &&
                      compilationDirectories.empty();
  Json result{{"enabled", !settings.directories.empty()}, {"running", running},
              {"active", active}, {"pending", dirty}, {"directories", directories},
              {"scanning", scanning}, {"ready", ready},
              {"events", events}, {"cycles", cycles}, {"failures", failures},
              {"overflows", overflows}, {"last_error", error},
              {"latest_jobs", latestJobs},
              {"import_mode", stored ? "stored_commands" : "reimport"}};
  if (stored && !settings.directories.empty())
    result["notice"] = "No compilation database discovered: extracts stored "
                       "commands. Import new translation units explicitly or "
                       "configure import_arguments.";
#ifdef __linux__
  result["backend"] = "inotify";
  result["watched_directories"] = watches.size();
#else
  result["backend"] = "unsupported";
  result["watched_directories"] = 0;
#endif
  return result;
}
}
