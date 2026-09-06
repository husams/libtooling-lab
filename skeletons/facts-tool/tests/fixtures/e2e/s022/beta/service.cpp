#include "service.hpp"

namespace s022_fixture {

int helper() { return 1; }

Base::~Base() { helper(); }

int Base::value() const { return helper(); }

Member::~Member() { helper(); }

Derived::Derived() { helper(); }

Derived::~Derived() { helper(); }

int Derived::value() const { return helper() + 1; }

int invoke(Base *value) { return value->value(); }

int consume(ImplicitValue value) { return value.value; }

} // namespace s022_fixture
