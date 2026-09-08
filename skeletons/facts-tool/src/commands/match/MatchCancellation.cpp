#include "commands/match/MatchCancellation.h"

namespace facts::commands::match {
namespace {
volatile std::sig_atomic_t interrupted = 0;

void interrupt(int) { interrupted = 1; }
} // namespace

MatchCancellation::MatchCancellation()
    : previous_(std::signal(SIGINT, interrupt)) {
  interrupted = 0;
}

MatchCancellation::~MatchCancellation() {
  std::signal(SIGINT, previous_);
  interrupted = 0;
}

bool MatchCancellation::cancelled() { return interrupted != 0; }

} // namespace facts::commands::match
