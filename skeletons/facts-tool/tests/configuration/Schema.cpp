#include "Sandbox.h"
#include <array>

namespace configuration_test {
void schema() {
  Sandbox box;
  const std::array invalid{
      "[]", "conf_root: null", "conf_root: ''", "conf_root: []",
      "conf_template: null", "conf_template: {}", "extra_args: null",
      "extra_args: {}", "extra_args: scalar", "extra_args: [null]",
      "extra_args: [3]", "extra_args: [true]", "extra_args: [[]]",
      "extra_args: [!!str text]", "extra_args: &anchor []",
      "extra_args: [*anchor]", "extra_args: []\nextra_args: []",
      "{}\n---\n{}", "unknown: x", "extra_args: [bad",
      R"(extra_args: ["a\0b"])", R"(extra_args: ["a\nb"])",
      R"(conf_root: "a\0b")", R"(conf_template: "a\nb")"};
  for (const auto yaml : invalid) {
    const auto parsed = facts::config::readYaml(box.write("invalid.yaml", yaml), {});
    assert(!parsed);
  }
  for (const auto yaml : {"", "{}", "extra_args: []", "extra_args: ['']",
                         "extra_args: ['123', 'true', '-Iwith spaces']"}) {
    assert(facts::config::readYaml(box.write("valid.yaml", yaml), {}));
  }
  auto parsed = facts::config::readYaml(box.write("tokens.yaml",
      "extra_args: ['-include', 'space header.hpp', '-DVALUE=1', '-DVALUE=1']"), {});
  assert(parsed && parsed->extraArguments == std::vector<std::string>(
      {"-include", "space header.hpp", "-DVALUE=1", "-DVALUE=1"}));
  assert(parsed->extraArgumentsSource == (box.root / "tokens.yaml").string() + ": extra_args");
  facts::config::Resolved direct;
  direct.generated = false;
  assert(facts::config::readYaml(box.write("unused.yaml",
      "conf_root: null\nconf_template: []\nextra_args: []"), direct));
}
}
