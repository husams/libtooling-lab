#include "api.hpp"

namespace app {

template struct Box<int, 7>;
template struct Holder<int, 4>;

int configured = 42;
int persist() { return 1; }
int save() { return persist(); }

int run(Box<int, 7> box) noexcept {
  return save() + save() + box.value;
}

int dispatch_probe(Box<int, 7> &box) {
  box.flush();
  return box.value;
}

int diamond_end() { return 1; }
int diamond_left() { return diamond_end(); }
int diamond_right() { return diamond_end(); }
int diamond_source() { return diamond_left() + diamond_right(); }

int cycle_a() { return cycle_b(); }
int cycle_b() { return cycle_a(); }

}  // namespace app
