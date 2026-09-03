#include <string>
#include <new>

struct alignas(64) RuntimeAllocatedAgain {};

void reproduceExternalCalleeAgain() {
  std::string value = "temporary";
  delete new RuntimeAllocatedAgain;
}

void releaseAlignedAgain(void *memory) {
  ::operator delete(memory, sizeof(RuntimeAllocatedAgain),
                    std::align_val_t{64});
}
