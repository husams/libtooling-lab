#include "apis/jobs/Spawn.h"
#include <array>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <unistd.h>

extern char **environ;
namespace facts::apis {
namespace {
struct Pipes {
  std::array<int, 4> descriptors{-1, -1, -1, -1};
  ~Pipes() { for (int fd : descriptors) if (fd >= 0) ::close(fd); }
};
int createPipe(int *fds) {
#if defined(__linux__)
  if (::pipe2(fds, O_CLOEXEC) != 0) return errno;
#else
  if (::pipe(fds) != 0) return errno;
  for (int index = 0; index < 2; ++index)
    if (::fcntl(fds[index], F_SETFD, FD_CLOEXEC) < 0) return errno;
#endif
  for (int index = 0; index < 2; ++index) {
    if (fds[index] >= 3) continue;
    int replacement = ::fcntl(fds[index], F_DUPFD_CLOEXEC, 3);
    if (replacement < 0) return errno;
    ::close(fds[index]);
    fds[index] = replacement;
  }
  return 0;
}
struct SpawnOptions {
  posix_spawn_file_actions_t actions;
  posix_spawnattr_t attributes;
  bool hasActions = false, hasAttributes = false;
  ~SpawnOptions() {
    if (hasActions) posix_spawn_file_actions_destroy(&actions);
    if (hasAttributes) posix_spawnattr_destroy(&attributes);
  }
  int initialize(const Pipes &pipes) {
    int error = posix_spawn_file_actions_init(&actions);
    if (error) return error;
    hasActions = true;
    error = posix_spawnattr_init(&attributes);
    if (error) return error;
    hasAttributes = true;
    if ((error = posix_spawn_file_actions_addopen(
             &actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0))) return error;
    if ((error = posix_spawn_file_actions_adddup2(
             &actions, pipes.descriptors[1], STDOUT_FILENO))) return error;
    if ((error = posix_spawn_file_actions_adddup2(
             &actions, pipes.descriptors[3], STDERR_FILENO))) return error;
    for (int fd : pipes.descriptors)
      if ((error = posix_spawn_file_actions_addclose(&actions, fd))) return error;
    sigset_t empty, defaults;
    sigemptyset(&empty);
    sigemptyset(&defaults);
    for (int signal : {SIGINT, SIGTERM, SIGHUP, SIGPIPE}) sigaddset(&defaults, signal);
    if ((error = posix_spawnattr_setsigmask(&attributes, &empty))) return error;
    if ((error = posix_spawnattr_setsigdefault(&attributes, &defaults))) return error;
    if ((error = posix_spawnattr_setpgroup(&attributes, 0))) return error;
    return posix_spawnattr_setflags(&attributes,
        POSIX_SPAWN_SETPGROUP | POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSIGDEF);
  }
};
}
std::expected<Child, std::string> spawnChild(
    const std::filesystem::path &executable,
    const std::vector<std::string> &arguments) {
  Pipes pipes;
  int error = createPipe(pipes.descriptors.data());
  if (!error) error = createPipe(pipes.descriptors.data() + 2);
  if (error) return std::unexpected(std::strerror(error));
  SpawnOptions options;
  if ((error = options.initialize(pipes)))
    return std::unexpected(std::strerror(error));
  std::string command = executable.string();
  std::vector<char *> argv{command.data()};
  for (const auto &argument : arguments)
    argv.push_back(const_cast<char *>(argument.c_str()));
  argv.push_back(nullptr);
  pid_t pid = -1;
  error = ::posix_spawn(&pid, command.c_str(), &options.actions,
                       &options.attributes, argv.data(), environ);
  if (error) return std::unexpected(std::strerror(error));
  Child child{pid, pipes.descriptors[0], pipes.descriptors[2]};
  pipes.descriptors[0] = pipes.descriptors[2] = -1;
  return child;
}
}
