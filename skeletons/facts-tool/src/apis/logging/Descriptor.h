#pragma once
#include <unistd.h>

namespace facts::apis::logging {
struct Descriptor {
  int value = -1;
  ~Descriptor() { if (value >= 0) ::close(value); }
  Descriptor() = default;
  explicit Descriptor(int descriptor) : value(descriptor) {}
  Descriptor(const Descriptor &) = delete;
  Descriptor &operator=(const Descriptor &) = delete;
  void reset(int descriptor) {
    if (value >= 0) ::close(value);
    value = descriptor;
  }
  int release() { const int result = value; value = -1; return result; }
};
}
