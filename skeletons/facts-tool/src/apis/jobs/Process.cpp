#include "apis/jobs/Process.h"
#include "apis/jobs/Spawn.h"
#include <cerrno>
#include <csignal>
#include <sys/wait.h>

namespace facts::apis {
Process::Process(boost::asio::io_context &io, std::filesystem::path executable,
                 std::vector<std::string> arguments, unsigned timeout,
                 Completion completion, std::filesystem::path workingDirectory)
    : executable_(std::move(executable)), workingDirectory_(std::move(workingDirectory)), arguments_(std::move(arguments)),
      timeout_(timeout), completion_(std::move(completion)), output_(io),
      error_(io), pollTimer_(io), deadline_(io), killTimer_(io), drainTimer_(io) {}

void Process::start() {
  auto child = spawnChild(executable_, arguments_, workingDirectory_);
  if (!child) {
    error_.text = "Cannot start command: " + child.error();
    exited_ = true;
    closeStreams();
    finish();
    return;
  }
  pid_ = child->pid;
  output_.descriptor.assign(child->output);
  error_.descriptor.assign(child->error);
  read(output_);
  read(error_);
  poll();
  if (!timeout_) return;
  deadline_.expires_after(std::chrono::seconds(timeout_));
  deadline_.async_wait([self = shared_from_this()](auto error) {
    if (error || self->exited_ || self->done_) return;
    self->timedOut_ = true;
    self->terminate();
  });
}
void Process::cancel(bool force) {
  if (done_ || (cancelled_ && !force)) return;
  cancelled_ = true;
  if (!force) { terminate(); return; }
  if (pid_ > 0 && !exited_) {
    ::kill(-pid_, SIGKILL);
    int status = 0;
    pid_t result;
    do { result = ::waitpid(pid_, &status, 0); } while (result < 0 && errno == EINTR);
    exitCode_ = result == pid_ && WIFEXITED(status) ? WEXITSTATUS(status) : 137;
  }
  exited_ = true;
  output_.truncated |= output_.descriptor.is_open();
  error_.truncated |= error_.descriptor.is_open();
  closeStreams();
  finish();
}
void Process::terminate() {
  deadline_.cancel();
  if (exited_) { closeStreams(); finish(); return; }
  if (pid_ <= 0) return;
  ::kill(-pid_, SIGTERM);
  killTimer_.expires_after(std::chrono::milliseconds(500));
  killTimer_.async_wait([self = shared_from_this()](auto error) {
    if (!error && !self->exited_ && !self->done_) ::kill(-self->pid_, SIGKILL);
  });
}
void Process::finish() {
  if (done_ || !exited_ || !output_.closed || !error_.closed) return;
  done_ = true;
  pollTimer_.cancel();
  deadline_.cancel();
  killTimer_.cancel();
  drainTimer_.cancel();
  auto completion = std::move(completion_);
  if (completion) completion({exitCode_, std::move(output_.text),
      std::move(error_.text), output_.truncated || error_.truncated,
      cancelled_, timedOut_});
}
}
