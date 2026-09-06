#include "b040_boundary.hpp"
#include <b040_external.hpp>

namespace b040_fixture {
void root() {
  missing();
  known_mid();
  leaf_only();
  b040_external::unavailable();
}
} // namespace b040_fixture
