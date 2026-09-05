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
      "compile consumers still load YAML extra_args");
  add("--config", selector,
      "YAML defaults (yaml-cpp 0.9.0): first file wins, --config > FACTS_TOOL_CONFIG > "
      "canonical cwd project .facts-tool.yaml > XDG_CONFIG_HOME/facts-tool/config.yaml "
      "or HOME/.config/facts-tool/config.yaml; no merging. Built-ins: "
      "XDG_DATA_HOME/facts-tool or HOME/.local/share/facts-tool, "
      "{relative_path}/{filename}.db, extra_args=[]. Relative conf_root anchors to YAML; "
      "only ~/ expands. Canonical generated paths must stay below conf_root and "
      "belong to one project. Use config show for provenance without mutation.");
}
} // namespace facts::cli
