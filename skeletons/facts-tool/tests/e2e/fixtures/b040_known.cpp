#include "b040_boundary.hpp"

namespace b040_fixture {
void known_leaf() {}
void known_mid() { known_leaf(); }
} // namespace b040_fixture
