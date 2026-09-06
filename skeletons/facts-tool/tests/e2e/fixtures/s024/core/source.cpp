#include "api.hpp"

namespace s024_fixture {
void source(Base &base) {
  alpha(base);
  beta(base);
  dispatch(base);
}

void onlyCaller(Base &base) { source(base); }

int overloaded(int value) { return value; }

double overloaded(double value) { return value; }
} // namespace s024_fixture
