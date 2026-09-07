#include "api.h"

namespace s027 {
int leaf() { return 1; }

int shared() { return leaf(); }

int left() { return shared(); }

int right() { return shared(); }

int cycle_a(int value) { return value ? cycle_b(value - 1) : leaf(); }

int cycle_b(int value) { return value ? cycle_a(value - 1) : leaf(); }

int boundary() { return external(); }

int indirect(int (*callback)()) { return callback(); }

int overloaded(int value) { return value; }

int overloaded(double value) { return static_cast<int>(value); }
} // namespace s027
