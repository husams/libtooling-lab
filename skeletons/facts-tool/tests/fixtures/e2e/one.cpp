#include "shared.hpp"

namespace e2e {

int sharedCounter = 3;

int useOne() { return transform(Widget{2}); }

} // namespace e2e
