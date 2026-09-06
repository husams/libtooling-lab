#include "recovery_attempts_test_support.h"

#include <filesystem>
#include <fstream>

namespace recovery_attempts_test {
void runDigestCacheTests(const Fixture &fixture) {
  const auto input = fixture.input();
  InputDigestCache cache;
  const auto first = cache.digest(input.inputs);
  const auto second = cache.digest(input.inputs);
  assert(first && second && *first == *second);
  assert(cache.readCount() == 2);
  const auto source_time = std::filesystem::last_write_time(fixture.source);
  std::ofstream(fixture.source) << "int source = 2;\n";
  std::filesystem::last_write_time(fixture.source, source_time);
  const auto changed = cache.digest(input.inputs);
  assert(changed && *changed != *first);
  assert(cache.readCount() == 3);
  std::ofstream(fixture.source) << "int source = 1;\n";
  std::filesystem::last_write_time(fixture.source, source_time);
}

void runPreAttemptTest(AttemptCache &cache, const AttemptInput &input,
                       const Fixture &fixture) {
  auto pre_attempt = cache.build(input);
  std::ofstream(fixture.source) << "z";
  assert(pre_attempt && cache.record(*pre_attempt, input.requested_usrs,
                                     AttemptOutcome::failed,
                                     {"compile", "changed during attempt"}));
  assert(miss(cache, input));
}
} // namespace recovery_attempts_test
