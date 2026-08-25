#pragma once

namespace reference_fixture {

inline int primaryTarget = 1;
inline int secondaryTarget = 2;

inline int helper(int value) { return value; }

struct Constructed {
  explicit Constructed(int value) : value(value) {}

  int value;
};

struct Example {
  int field = 3;

  int method() const { return primaryTarget + field; }
};

inline int sharedInline() {
  return primaryTarget + primaryTarget + secondaryTarget +
         helper(primaryTarget) + Constructed{primaryTarget}.value;
}

template <typename T>
int templatedOwner() {
  return primaryTarget;
}

template <>
inline int templatedOwner<long>() {
  return secondaryTarget;
}

inline int redeclaredOwner();

inline int redeclaredOwner() { return primaryTarget; }

inline int nestedDeclarations() {
  struct Local {
    int localField = 4;

    int method() const { return localField + primaryTarget; }
  };

  enum LocalEnum { LocalAlpha, LocalBeta };

  auto lambda = [] { return secondaryTarget; };
  auto sibling = [] { return primaryTarget; };
  return Local{}.method() + lambda() + sibling() + LocalAlpha + LocalBeta;
}

} // namespace reference_fixture
