#pragma once

namespace call_graph_fixture {

struct Base {
  virtual ~Base() = default;
  virtual int toString() const;
  int log() const;
};

struct X final : Base { int toString() const override; };
struct Y final : Base { int toString() const override; };

int declarationOnly(int value);
int externalOnly(int value);

template <typename T>
int invoke(T &value) {
  return value.log();
}

} // namespace call_graph_fixture
