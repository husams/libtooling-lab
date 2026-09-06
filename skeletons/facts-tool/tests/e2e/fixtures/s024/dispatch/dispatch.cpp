#include "api.hpp"

namespace s024_fixture {
void Base::invoke() {}

void Left::invoke() { target(); }

void Right::invoke() { target(); }

void dispatch(Base &base) { base.invoke(); }
} // namespace s024_fixture
