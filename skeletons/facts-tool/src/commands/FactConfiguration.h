#pragma once

#include "commands/ConfigurationSupport.h"

namespace facts::commands {

struct FactConfiguration {
  config::Resolved project;
  std::filesystem::path facts;
  bool projectSelected = false;
};

// A facts-path override does not override the project database. Discover the
// same defaults as import/extract before deciding whether a standalone facts
// database has a project to pair with. A direct/selected configuration, YAML
// project path settings, or an existing generated database selects the project;
// facts/compiler/cache settings alone do not. A selected project must never
// silently fall back when its database is missing.
inline std::expected<FactConfiguration, std::string>
loadFactConfiguration(const std::string &direct, const std::string &selector,
                      const std::string &facts,
                      const std::vector<std::string> &sources = {},
                      bool compilerDefaults = false) {
  return loadConfiguration(direct, selector, false,
                           compilerDefaults || facts.empty())
      .and_then([&](config::Resolved project)
                    -> std::expected<FactConfiguration, std::string> {
        std::error_code error;
        const bool exists = std::filesystem::exists(project.database, error);
        if (error)
          return std::unexpected("facts-tool: configuration error: cannot "
                                 "inspect project database: " + error.message());
        const bool selected =
            !project.generated || !selector.empty() ||
            config::detail::present("FACTS_TOOL_CONFIG") ||
            project.storageRootSource != "built-in" ||
            project.templateSource != "built-in" || exists;
        if (selected && !exists)
          return std::unexpected("project configuration database not found: " +
                                 project.database.string());
        auto output = facts.empty()
                          ? resolveFactsOutput(project, sources)
                          : std::expected<std::filesystem::path, std::string>{facts};
        return output.transform([&](auto path) {
          return FactConfiguration{std::move(project), std::move(path), selected};
        });
      });
}

} // namespace facts::commands
