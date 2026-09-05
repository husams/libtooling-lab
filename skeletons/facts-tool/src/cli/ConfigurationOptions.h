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
      "compile consumers still load and merge YAML extra_args");
  add("--config", selector,
      "YAML defaults (yaml-cpp 0.9.0) merge per key, highest precedence first: "
      "--config/FACTS_TOOL_CONFIG file, the nearest project .facts-tool.yaml, then "
      "the user file at XDG_CONFIG_HOME/facts-tool/config.yaml or "
      "HOME/.config/facts-tool/config.yaml; extra_args instead concatenate user, "
      "then project, then --config tokens, then CLI --extra-arg. Built-ins: "
      "XDG_DATA_HOME/facts-tool or HOME/.local/share/facts-tool, "
      "conf_template={relative_path}/{filename}.db, no facts_template, extra_args=[]. "
      "Placeholders in conf_template/facts_template: {project_root}, {project_name}, "
      "{relative_path}, {filename}, {user}, and ${ENV_NAME}. A relative conf_root or "
      "template result anchors to the canonical project root, never to a YAML file's "
      "directory; a raw literal absolute template is rejected, but a leading ~/ or a "
      "placeholder (e.g. {project_root}) may make the result absolute, trusted as-is "
      "aside from a symlink escaping through it. facts_template supplies the default "
      "--facts/-o for extract, analyse dependency, and symbol when omitted, and is "
      "never bypassed by --conf/FACTS_TOOL_CONF. A relative conf_template result must "
      "stay below conf_root and belong to one project. "
      "Use config show for per-key provenance without mutation.");
}
} // namespace facts::cli
