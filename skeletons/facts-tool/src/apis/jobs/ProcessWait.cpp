#include "apis/jobs/Process.h"
#include <cerrno>
#include <csignal>
#include <sys/wait.h>

namespace facts::apis {
void Process::poll() {
  if (done_ || exited_) return;
  int status = 0;
  pid_t result = ::waitpid(pid_, &status, WNOHANG);
  if (result == pid_) { reaped(status); return; }
  if (result < 0 && errno != EINTR) {
    exited_ = true;
    closeStreams();
    finish();
    return;
  }
  pollTimer_.expires_after(std::chrono::milliseconds(25));
  pollTimer_.async_wait([self = shared_from_this()](auto error) {
    if (!error) self->poll();
  });
}
void Process::reaped(int status) {
  // A command may exit on SIGTERM before its own subprocesses do.
  if (cancelled_ || timedOut_) ::kill(-pid_, SIGKILL);
  exited_ = true;
  exitCode_ = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
  deadline_.cancel();
  killTimer_.cancel();
  finish();
  if (done_) return;
  // Descendants may retain stdout/stderr after the direct child exits.
  drainTimer_.expires_after(std::chrono::milliseconds(250));
  drainTimer_.async_wait([self = shared_from_this()](auto error) {
    if (error || self->done_) return;
    self->output_.truncated |= !self->output_.closed;
    self->error_.truncated |= !self->error_.closed;
    self->closeStreams();
    self->finish();
  });
}
}
