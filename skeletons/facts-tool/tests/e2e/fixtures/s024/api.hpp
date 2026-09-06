#pragma once

namespace s024_fixture {
struct Base {
  virtual void invoke();
};

struct Left final : Base {
  void invoke() override;
};

struct Right final : Base {
  void invoke() override;
};

void target();
void isolated();
void source(Base &base);
void alpha(Base &base);
void beta(Base &base);
void dispatch(Base &base);
void onlyCaller(Base &base);
int overloaded(int value);
double overloaded(double value);
} // namespace s024_fixture
