#pragma once
namespace qualifiers {
struct Cv {
  int plain();
  int plain() const;
  int plain() volatile;
  int plain() const volatile;
  int ref() &;
  int ref() const &;
  int ref() volatile &;
  int ref() const volatile &;
  int ref() &&;
  int ref() const &&;
  int ref() volatile &&;
  int ref() const volatile &&;
  static const int &staticControl(const int &value) { return value; }
  int split(const int &value = 7) const volatile & noexcept;
};
inline const int &freeControl(const int &value) { return value; }
}
