#include "apis/watch/State.h"
#include "apis/watch/plan/Plan.h"

namespace facts::apis {
Json Watcher::Impl::status() const {
  Json roots = Json::array(), clones = Json::array(), notices = Json::array();
  std::string database;
  if (snapshot) {
    database = snapshot->catalog.database.string();
    for (const auto &path : snapshot->roots) roots.push_back(path.string());
    for (const auto &clone : snapshot->catalog.clones)
      clones.push_back({{"repository_id", clone.repositoryId},
        {"repository", clone.repository}, {"clone_id", clone.cloneId},
        {"label", clone.label}, {"path", clone.path.string()},
        {"active", clone.active}, {"excluded", clone.excluded}});
    notices = snapshot->notices;
  }
  const auto explicitPaths = watch::explicitDatabases(settings);
  const bool stored = explicitPaths && explicitPaths->empty() &&
                      (!snapshot || snapshot->databases.empty());
  Json result{{"enabled", settings.watchEnabled}, {"running", running},
              {"active", active}, {"pending", dirty}, {"directories", roots},
              {"scanning", scanning}, {"ready", ready},
              {"source", "project_database"}, {"project_database", database},
              {"clones", clones}, {"notices", notices},
              {"events", events}, {"cycles", cycles}, {"failures", failures},
              {"overflows", overflows}, {"last_error", error},
              {"latest_jobs", latestJobs},
              {"import_mode", stored ? "stored_commands" : "reimport"}};
  if (stored && !roots.empty())
    result["notice"] = "No compilation database discovered: extracts eligible stored "
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
