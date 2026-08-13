#pragma once

namespace e2e {

struct Widget {
  int value;
};

enum class Mode { Fast, Slow };

using Count = int;

extern int sharedCounter;

inline int headerHelper(int input, int delta = 1) { return input + delta; }

int transform(const Widget &widget, Count factor = 2);

} // namespace e2e
