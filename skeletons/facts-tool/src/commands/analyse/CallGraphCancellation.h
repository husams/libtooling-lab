#pragma once

#include <csignal>

namespace facts::commands {

class CallGraphCancellation {
public:
  CallGraphCancellation();
  ~CallGraphCancellation();
  CallGraphCancellation(const CallGraphCancellation &) = delete;
  CallGraphCancellation &operator=(const CallGraphCancellation &) = delete;
  static bool cancelled();

private:
  using Handler = void (*)(int);
  Handler previous_;
};

} // namespace facts::commands
