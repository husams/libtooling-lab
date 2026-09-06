#include <new>

void *case_nothrow_new(decltype(sizeof(0)) n) {
  return ::operator new(n, std::nothrow);
}

void *case_nothrow_array_new(decltype(sizeof(0)) n) {
  return ::operator new[](n, std::nothrow);
}

void *case_nothrow_aligned_new(decltype(sizeof(0)) n) {
  return ::operator new(n, std::align_val_t{64}, std::nothrow);
}

void *case_nothrow_aligned_array_new(decltype(sizeof(0)) n) {
  return ::operator new[](n, std::align_val_t{64}, std::nothrow);
}

void case_nothrow_delete(void *p) { ::operator delete(p, std::nothrow); }

void case_nothrow_array_delete(void *p) {
  ::operator delete[](p, std::nothrow);
}

void case_nothrow_aligned_delete(void *p) {
  ::operator delete(p, std::align_val_t{64}, std::nothrow);
}

void case_nothrow_aligned_array_delete(void *p) {
  ::operator delete[](p, std::align_val_t{64}, std::nothrow);
}

void *case_builtin(void *p, const void *q, decltype(sizeof(0)) n) {
  return __builtin_memcpy(p, q, n);
}
