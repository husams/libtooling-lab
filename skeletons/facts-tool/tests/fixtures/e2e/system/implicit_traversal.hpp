#pragma GCC system_header
inline int *traversal_seed() { return new int; }
inline void *traversal_runtime(decltype(sizeof(0)) n) {
  return ::operator new(n);
}
