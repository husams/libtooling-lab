#include <sstream>
#include <string>

namespace b022 {

void sibling() {}

void canary(const std::string &value) {
  std::ostringstream stream;
  stream << "value retrieved from shared memory is " << value;
  const std::string message = stream.str();
  (void)message;
  sibling();
}

} // namespace b022
