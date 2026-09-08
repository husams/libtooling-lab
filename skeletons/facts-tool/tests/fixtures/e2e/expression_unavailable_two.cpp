#define TOUCH(value) ((value) = 2)

namespace unavailable_two {
struct Record {
  int field = 0;
  void set() { TOUCH(field); }
};
} // namespace unavailable_two
