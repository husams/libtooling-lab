#include "b040_boundary.hpp"
#include "external/b040_external.hpp"

namespace b040_fixture {
void root() {
  missing();
  known_mid();
  b040_external::unavailable();
}
} // namespace b040_fixture
