#include "Sandbox.h"
#include <array>

namespace configuration_test {
void schema() {
  Sandbox box;
  const std::array invalid{
      "[]", "conf_root: null", "conf_root: ''", "conf_root: []",
      "conf_template: null", "conf_template: {}", "facts_template: null",
      "facts_template: {}", "extra_args: null",
      "extra_args: {}", "extra_args: scalar", "extra_args: [null]",
      "extra_args: [3]", "extra_args: [true]", "extra_args: [[]]",
      "extra_args: [!!str text]", "extra_args: &anchor []",
      "extra_args: [*anchor]", "extra_args: []\nextra_args: []",
      "{}\n---\n{}", "unknown: x", "extra_args: [bad",
      R"(extra_args: ["a\0b"])", R"(extra_args: ["a\nb"])",
      R"(conf_root: "a\0b")", R"(conf_template: "a\nb")"};
  for (const auto yaml : invalid) {
    const auto parsed = facts::config::readTier(box.write("invalid.yaml", yaml), true);
    assert(!parsed);
  }
  for (const auto yaml : {"", "{}", "extra_args: []", "extra_args: ['']",
                         "extra_args: ['123', 'true', '-Iwith spaces']",
                         "facts_template: 'a.db'"}) {
    assert(facts::config::readTier(box.write("valid.yaml", yaml), true));
  }
  auto parsed = facts::config::readTier(box.write("tokens.yaml",
      "extra_args: ['-include', 'space header.hpp', '-DVALUE=1', '-DVALUE=1']"), true);
  assert(parsed && parsed->extraArgs == std::vector<std::string>(
      {"-include", "space header.hpp", "-DVALUE=1", "-DVALUE=1"}));
  assert(!parsed->confRoot && !parsed->confTemplate && !parsed->factsTemplate);

  // When path settings do not apply (a direct db override is active),
  // conf_root/conf_template are left unpopulated and unvalidated, but
  // extra_args and facts_template (B-030 C-3113: it governs the separate
  // facts database, not the overridden project conf database) are always
  // required to be well-formed.
  auto unused = facts::config::readTier(box.write("unused.yaml",
      "conf_root: null\nconf_template: []\nextra_args: []"), false);
  assert(unused && !unused->confRoot && !unused->confTemplate);
  auto stillChecked = facts::config::readTier(box.write("bad-args.yaml",
      "conf_root: null\nextra_args: [1]"), false);
  assert(!stillChecked);
  auto factsAlwaysValidated = facts::config::readTier(box.write("bad-facts-template.yaml",
      "conf_root: null\nfacts_template: 3"), false);
  assert(!factsAlwaysValidated);
  auto factsAlwaysCaptured = facts::config::readTier(box.write("facts-template.yaml",
      "conf_root: null\nfacts_template: 'a.db'"), false);
  assert(factsAlwaysCaptured && !factsAlwaysCaptured->confRoot &&
        factsAlwaysCaptured->factsTemplate == "a.db");

  // Template syntax (unknown placeholder, unmatched braces, unset ${ENV})
  // is validated per tier, independent of which tier wins the merge later.
  assert(!facts::config::readTier(box.write("bad-template-syntax.yaml",
      "conf_template: '{unknown}.db'"), true));
  assert(!facts::config::readTier(box.write("bad-facts-syntax.yaml",
      "facts_template: '{unmatched'"), true));
  unsetenv("FACTS_TOOL_TEST_UNSET_TOKEN");
  assert(!facts::config::readTier(box.write("bad-env-syntax.yaml",
      "conf_template: '${FACTS_TOOL_TEST_UNSET_TOKEN}.db'"), true));
}
}
