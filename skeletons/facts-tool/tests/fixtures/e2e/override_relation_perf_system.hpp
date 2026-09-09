#pragma once
#pragma clang system_header

namespace override_relation_perf {

struct SystemBase {
  virtual int value() const { return 1; }
};

struct SystemDerived : SystemBase {
  int value() const override { return 2; }
};

} // namespace override_relation_perf
