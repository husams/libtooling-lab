#pragma once

namespace app {

struct Base {
  virtual void flush() = 0;
};

template <typename T, int... Args> struct Box : Base {
  T value = 42;
  void flush() override {}
};

template <typename T, int N> struct Holder {
  T values[N]{};
};

extern template struct Box<int, 7>;
extern template struct Holder<int, 4>;

enum class Color : int { Red = 1 };

extern int configured;
int persist();
int save();
int run(Box<int, 7> box = {}) noexcept;
int dispatch_probe(Box<int, 7> &box);
int diamond_end();
int diamond_left();
int diamond_right();
int diamond_source();
int cycle_a();
int cycle_b();

}  // namespace app
