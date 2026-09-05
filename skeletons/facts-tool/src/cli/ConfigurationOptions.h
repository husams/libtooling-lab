#pragma once
#include <CLI/CLI.hpp>
#include <string>

namespace facts::cli {
inline void configurationOptions(CLI::App &command, std::string &direct,
                                 std::string &selector) {
  const auto add = [&](const char *name, std::string &target, const char *help) {
    command.add_option_function<std::string>(name,
        [&target, name](const auto &value) {
          if (value.empty()) throw CLI::ValidationError(std::string(name) + " must not be empty");
          target = value;
        }, help)->trigger_on_parse()->type_name("FILE");
  };
  add("-c,--conf", direct,
      "Direct project DB: overrides FACTS_TOOL_CONF and generated naming/ownership; "
      "compiler extras use YAML when CLI --extra-arg is omitted");
  add("--config", selector,
      "YAML defaults (yaml-cpp 0.9.0); every explicitly supplied CLI value "
      "overrides its YAML value, while omitted CLI values fall back. Files "
      "merge per key, highest precedence first: "
      "--config/FACTS_TOOL_CONFIG file, the nearest project .facts-tool.yaml, then "
      "the user file at XDG_CONFIG_HOME/facts-tool/config.yaml or "
      "HOME/.config/facts-tool/config.yaml; explicit --extra-arg occurrences "
      "replace the complete YAML extra_args list. Without CLI extras, YAML "
      "tokens concatenate user, then project, then selected file. Built-ins: "
      "XDG_DATA_HOME/facts-tool or HOME/.local/share/facts-tool, "
      "conf_template={relative_path}/{filename}.db, and extra_args=[]. "
      "Placeholders in conf_template/facts_template: {project_root}, "
      "{project_name}, {relative_path}, {filename}, {user}, and ${ENV_NAME}. "
      "facts_template supplies --output/-o, --facts/-f when omitted. "
      "Use config show for per-key provenance without mutation.");
}
} // namespace facts::cli
