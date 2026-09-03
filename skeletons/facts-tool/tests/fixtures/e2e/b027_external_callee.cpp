#include <string>
#include <new>

struct alignas(64) RuntimeAllocated {};

void reproduceExternalCallee() {
  std::string value = "temporary";
  delete new RuntimeAllocated;
}

void releaseAligned(void *memory) {
  ::operator delete(memory, sizeof(RuntimeAllocated), std::align_val_t{64});
}
