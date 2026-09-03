#include <new>
#include <sstream>
#include <string>

struct alignas(64) RuntimeAllocatedAgain {};

std::string reproduceStreamTemporary(const std::string &value) {
  std::ostringstream stream;
  stream << "value=" << value;
  return stream.str();
}

void releaseAlignedAgain(void *memory) {
  ::operator delete(memory, sizeof(RuntimeAllocatedAgain),
                    std::align_val_t{64});
}
