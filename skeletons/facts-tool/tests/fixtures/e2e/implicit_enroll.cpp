#include "system/implicit_traversal.hpp"

void *enroll_runtime(decltype(sizeof(0)) n) { return traversal_runtime(n); }
