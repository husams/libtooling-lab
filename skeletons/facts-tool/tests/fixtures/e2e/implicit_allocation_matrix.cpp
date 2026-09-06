namespace std {
enum class align_val_t : decltype(sizeof(0));
}

struct alignas(64) Overaligned {
  int value;
};

int *seed_scalar() { return new int(7); }

int *seed_array() { return new int[3]; }

Overaligned *seed_aligned() { return new Overaligned; }

void *case_new(decltype(sizeof(0)) n) { return ::operator new(n); }

void *case_array_new(decltype(sizeof(0)) n) { return ::operator new[](n); }

void *case_aligned_new(decltype(sizeof(0)) n) {
  return ::operator new(n, std::align_val_t{64});
}

void *case_aligned_array_new(decltype(sizeof(0)) n) {
  return ::operator new[](n, std::align_val_t{64});
}

void case_delete(void *p) { ::operator delete(p); }

void case_array_delete(void *p) { ::operator delete[](p); }

void case_sized_delete(void *p, decltype(sizeof(0)) n) {
  ::operator delete(p, n);
}

void case_sized_array_delete(void *p, decltype(sizeof(0)) n) {
  ::operator delete[](p, n);
}

void case_aligned_delete(void *p) {
  ::operator delete(p, std::align_val_t{64});
}

void case_aligned_array_delete(void *p) {
  ::operator delete[](p, std::align_val_t{64});
}

void case_sized_aligned_delete(void *p, decltype(sizeof(0)) n) {
  ::operator delete(p, n, std::align_val_t{64});
}

void case_sized_aligned_array_delete(void *p, decltype(sizeof(0)) n) {
  ::operator delete[](p, n, std::align_val_t{64});
}
