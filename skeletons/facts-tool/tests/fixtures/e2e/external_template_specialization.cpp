#include <cstddef>
#include <functional>

namespace regression {

struct Hashable {
  int value;
};

} // namespace regression

namespace std {

template <>
struct hash<regression::Hashable> {
  size_t operator()(const regression::Hashable &value) const noexcept {
    return static_cast<size_t>(value.value);
  }
};

} // namespace std
