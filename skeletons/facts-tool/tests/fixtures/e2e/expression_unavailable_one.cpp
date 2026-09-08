#define TOUCH(value) ((value) = 1)

namespace unavailable_one {
struct Record {
  int field = 0;
  void set() { TOUCH(field); }
};
} // namespace unavailable_one
