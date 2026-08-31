#pragma once

#include <cstdint>

namespace facts {

enum class ReceiverCertainty : std::uint8_t {
  Exact = 1,
  Possible = 2,
};

} // namespace facts
