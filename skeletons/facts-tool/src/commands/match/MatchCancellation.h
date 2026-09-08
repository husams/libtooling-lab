#pragma once

#include <csignal>

namespace facts::commands::match {

class MatchCancellation {
public:
  MatchCancellation();
  ~MatchCancellation();
  MatchCancellation(const MatchCancellation &) = delete;
  MatchCancellation &operator=(const MatchCancellation &) = delete;

  static bool cancelled();

private:
  using Handler = void (*)(int);
  Handler previous_;
};

} // namespace facts::commands::match
