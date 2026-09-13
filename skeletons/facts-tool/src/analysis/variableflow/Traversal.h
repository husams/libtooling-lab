#pragma once

#include "analysis/variableflow/Summary.h"

#include <deque>
#include <map>
#include <memory>
#include <set>
#include <unordered_set>

namespace facts::variableflow::detail {

struct Binding {
  Summary *caller = nullptr;
  Summary *callee = nullptr;
  const Call *call = nullptr;
  unsigned argument = 0;
};

struct Traversal {
  const Parsed &parsed;
  const Request &request;
  Builder builder;
  std::unordered_map<std::string, std::unique_ptr<Summary>> summaries;
  std::unordered_map<std::int64_t, Summary *> owners;
  std::unordered_map<std::int64_t, std::vector<std::int64_t>> localLinks;
  std::unordered_map<std::int64_t, const Call *> calls;
  std::unordered_set<std::int64_t> active;
  std::unordered_map<std::int64_t, unsigned> expanded;
  std::deque<std::int64_t> pending;
  std::vector<Binding> bindings;
  std::map<std::tuple<std::int64_t, unsigned, bool>, std::int64_t> effectNodes;

  Summary &summary(const Function &, unsigned depth);
  void activate(std::int64_t);
  void drain();
  void expandCall(Summary &, const Call &);
  void linkEffects();
  void linkMemoryInput(Summary &, Summary &, const Call &, unsigned);
  std::int64_t callerEffect(Summary &, const Call &, unsigned argument,
                            bool memory);
  Graph finish();
};

} // namespace facts::variableflow::detail
