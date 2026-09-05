#pragma once
namespace qualifiers {
struct Throwing { ~Throwing() noexcept(false) {} };
struct ImplicitThrow { Throwing member; ~ImplicitThrow() = default; };
struct Nontrivial { ~Nontrivial() {} };
struct ImplicitSafe { Nontrivial member; ~ImplicitSafe() = default; };
struct FalseTrivial { ~FalseTrivial() noexcept(false) = default; };
static_assert(!noexcept(ImplicitThrow()));
static_assert(noexcept(ImplicitSafe()));
template<class T> T specialized(T value) noexcept { return value; }
template<> inline int specialized(int value) noexcept { return value; }
inline int useSpecialized() { return specialized(1) + specialized(2L); }
}
