#pragma once
namespace qualifiers {
int plain();
int safe() noexcept;
int safeTrue() noexcept(true);
int unsafe() noexcept(false);
constexpr int constant() { return 1; }
consteval int immediate() { return 2; }
int variadic(int first, ...);
static int internal() { return 1; }
extern int external();
namespace { int hidden() { return 0; } }
template<bool N> int conditional() noexcept(N) { return N; }
template int conditional<true>() noexcept(true);
template int conditional<false>() noexcept(false);
struct Specs {
  int plain();
  int safe() noexcept;
  int safeTrue() noexcept(true);
  int unsafe() noexcept(false);
  constexpr int constant() const { return 1; }
  consteval int immediate() const { return 2; }
  int variadic(int first, ...);
  template<bool N> int conditional() const & noexcept(N) { return N; }
  virtual int pure() = 0;
  virtual int overridden() { return 0; }
  void deleted() = delete;
};
struct Derived final : Specs {
  int pure() final { return 0; }
  int overridden() override { return 1; }
};
struct Special {
  Special() = default;
  explicit Special(int) noexcept {}
  Special(const Special &) = delete;
  ~Special() = default;
  explicit operator bool() const noexcept { return true; }
  explicit(false) operator int() const { return 1; }
};
template<bool N> struct Conditional {
  explicit(N) Conditional(int) noexcept(N) {}
  explicit(N) operator bool() const noexcept(N) { return N; }
};
inline void instantiate() {
  Derived d;
  d.conditional<true>();
  d.conditional<false>();
  Special s;
  Conditional<true> yes(1);
  Conditional<false> no(1);
  bool a = static_cast<bool>(yes), b = no;
  auto fixed = [](int x) noexcept { return x; };
  auto mutableLambda = [a](int x) mutable { return x; };
  auto generic = [](auto x) noexcept { return x; };
  auto mutableGeneric = [b](auto x) mutable { return x; };
  fixed(1); mutableLambda(1); generic(1); mutableGeneric(1);
}
}
