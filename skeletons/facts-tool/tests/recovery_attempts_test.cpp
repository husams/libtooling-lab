#include "recovery_attempts_test_support.h"
#include <filesystem>
#include <fstream>
#include <utility>
using namespace recovery_attempts_test;

int main() {
  Fixture fixture;
  AttemptCache cache;
  auto input = fixture.input();
  auto key = cache.build(input);
  assert(key && cache.record(*key, input.requested_usrs, AttemptOutcome::failed,
                             {"compile", "failed"}));
  auto reversed = input;
  std::swap(reversed.inputs[0], reversed.inputs[1]);
  auto reversed_key = cache.build(reversed);
  assert(reversed_key && reversed_key->input_digest == key->input_digest);
  expectHit(cache, input, AttemptOutcome::failed);
  input.requested_usrs.insert("usr:other");
  assert(miss(cache, input));
  input.requested_usrs = {"usr:target"};
  runDigestCacheTests(fixture);
  const auto source_time = std::filesystem::last_write_time(fixture.source);
  std::ofstream(fixture.source) << "int source = 2;\n";
  std::filesystem::last_write_time(fixture.source, source_time);
  assert(miss(cache, input));
  key = cache.build(input);
  input.requested_usrs.insert("usr:other");
  assert(key && cache.record(*key, input.requested_usrs,
                             AttemptOutcome::no_match, {"no-match", "none"}));
  expectHit(cache, input, AttemptOutcome::no_match);
  input.requested_usrs = {"usr:target"};
  expectHit(cache, input, AttemptOutcome::no_match);
  input.requested_usrs.insert("usr:new");
  assert(miss(cache, input));
  input.requested_usrs = {"usr:target"};
  input.translation_unit = 8;
  assert(miss(cache, input));
  input.translation_unit = 7;
  input.driver_identity = "/usr/bin/clang";
  assert(miss(cache, input));
  input.driver_identity = "/usr/bin/clang++";
  input.working_directory = fixture.other;
  assert(miss(cache, input));
  input.working_directory = fixture.root;
  input.registry_fingerprint = "registry-v2";
  assert(miss(cache, input));
  input.registry_fingerprint = "registry-v1";
  input.effective_argv.push_back("-DNEW");
  assert(miss(cache, input));
  input.effective_argv.pop_back();
  std::swap(input.effective_argv[0], input.effective_argv[1]);
  assert(miss(cache, input));
  std::swap(input.effective_argv[0], input.effective_argv[1]);
  auto alias = input;
  alias.pair.project = fixture.root / "project" / ".";
  expectHit(cache, alias, AttemptOutcome::no_match);
  alias.pair.project = fixture.other;
  assert(miss(cache, alias));
  expectHit(cache, input, AttemptOutcome::no_match);
  std::ofstream(fixture.facts) << "facts-v2";
  expectHit(cache, input, AttemptOutcome::no_match);
  std::ofstream(fixture.header) << "int header = 2;\n";
  assert(miss(cache, input));
  std::ofstream(fixture.header) << "int header = 1;\n";
  key = cache.build(input);
  assert(key && cache.record(*key, input.requested_usrs,
                             AttemptOutcome::succeeded, {"ok", "done"}));
  expectHit(cache, input, AttemptOutcome::succeeded);
  auto missing = input;
  missing.inputs.back().path = fixture.root / "missing.hpp";
  auto failure = cache.lookup(missing);
  assert(!failure && failure.error().code == AttemptErrorCode::missing_input);
  auto unreadable = input;
  unreadable.inputs.back().path = fixture.other;
  failure = cache.lookup(unreadable);
  assert(!failure &&
         failure.error().code == AttemptErrorCode::unreadable_input);
  auto empty = input;
  empty.inputs.clear();
  failure = cache.lookup(empty);
  assert(!failure && failure.error().code == AttemptErrorCode::invalid_input);
  auto omits_tu = input;
  omits_tu.inputs.erase(omits_tu.inputs.begin());
  failure = cache.lookup(omits_tu);
  assert(!failure && failure.error().code == AttemptErrorCode::invalid_input);
  AttemptCache fresh;
  assert(miss(fresh, input));
  std::ofstream(fixture.source) << "ab";
  std::ofstream(fixture.header) << "c";
  auto first = cache.build(input);
  std::ofstream(fixture.source) << "a";
  std::ofstream(fixture.header) << "bc";
  auto second = cache.build(input);
  assert(first && second && first->input_digest != second->input_digest);
  runPreAttemptTest(cache, input, fixture);
  return 0;
}
