#include "recovery_attempts_test_support.h"

#include "commands/analyse/RecoveryAttemptsFile.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

namespace recovery_attempts_test {
void rewriteUntilIdentityChanges(
    const std::filesystem::path &path, const char *content,
    std::optional<std::filesystem::file_time_type> keep_mtime) {
  const auto before = inspectInput(path.string());
  assert(before);
  for (;;) {
    std::ofstream(path) << content;
    if (keep_mtime)
      std::filesystem::last_write_time(path, *keep_mtime);
    const auto after = inspectInput(path.string());
    assert(after);
    if (keep_mtime)
      assert(after->size == before->size &&
             after->mtime_seconds == before->mtime_seconds &&
             after->mtime_nanoseconds == before->mtime_nanoseconds);
    if (*after != *before)
      return;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
}

void runDigestCacheTests(const Fixture &fixture) {
  const auto input = fixture.input();
  InputDigestCache cache;
  const auto first = cache.digest(input.inputs);
  const auto second = cache.digest(input.inputs);
  assert(first && second && *first == *second);
  assert(cache.readCount() == 2);
  const auto source_time = std::filesystem::last_write_time(fixture.source);
  rewriteUntilIdentityChanges(fixture.source, "int source = 2;\n", source_time);
  const auto changed = cache.digest(input.inputs);
  assert(changed && *changed != *first);
  assert(cache.readCount() == 3);
  std::ofstream(fixture.source) << "int source = 1;\n";
  std::filesystem::last_write_time(fixture.source, source_time);
}

void runPreAttemptTest(AttemptCache &cache, const AttemptInput &input,
                       const Fixture &fixture) {
  auto pre_attempt = cache.build(input);
  rewriteUntilIdentityChanges(fixture.source, "z");
  assert(pre_attempt && cache.record(*pre_attempt, input.requested_usrs,
                                     AttemptOutcome::failed,
                                     {"compile", "changed during attempt"}));
  assert(miss(cache, input));
}
} // namespace recovery_attempts_test
