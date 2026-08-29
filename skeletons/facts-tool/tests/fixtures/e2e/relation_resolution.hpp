#pragma once
#pragma GCC system_header

// Owners the extraction never persists: this header is a system header, so
// every declaration in it is filtered out of the fact store. The members
// defined out of line in relation_resolution.cpp still land in the main file,
// so their owner relations have to resolve a target the registry has not seen.

namespace regression {

struct Hashable {
  int key;
};

struct Box {
  static int value;
};

struct BoxedPair {
  static int value;
};

} // namespace regression

namespace std {

template <typename T> struct hash;

template <> struct hash<regression::Hashable> {
  unsigned long operator()(const regression::Hashable &value) const noexcept;
};

} // namespace std
