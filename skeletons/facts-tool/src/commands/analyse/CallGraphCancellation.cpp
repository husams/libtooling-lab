#include "commands/analyse/CallGraphCancellation.h"

namespace facts::commands {
namespace {
volatile std::sig_atomic_t interrupted = 0;

void interrupt(int) { interrupted = 1; }
} // namespace

CallGraphCancellation::CallGraphCancellation()
    : previous_(std::signal(SIGINT, interrupt)) {
  interrupted = 0;
}

CallGraphCancellation::~CallGraphCancellation() {
  std::signal(SIGINT, previous_);
  interrupted = 0;
}

bool CallGraphCancellation::cancelled() const { return interrupted != 0; }

} // namespace facts::commands
