#pragma once
#include <boost/asio/io_context.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <boost/asio/steady_timer.hpp>
#include <array>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <sys/types.h>
#include <vector>

namespace facts::apis {
struct ProcessResult {
  int exitCode = -1;
  std::string output = {}, error = {};
  bool truncated = false, cancelled = false, timedOut = false;
};
class Process : public std::enable_shared_from_this<Process> {
public:
  using Completion = std::function<void(ProcessResult)>;
  Process(boost::asio::io_context &io, std::filesystem::path executable,
          std::vector<std::string> arguments, unsigned timeout,
          Completion completion, std::filesystem::path workingDirectory = {});
  void start();
  void cancel(bool force = false);
private:
  struct Stream {
    explicit Stream(boost::asio::io_context &io) : descriptor(io) {}
    boost::asio::posix::stream_descriptor descriptor;
    std::array<char, 8192> buffer;
    std::string text;
    bool closed = false, truncated = false;
  };
  void read(Stream &stream);
  void closeStreams();
  void poll();
  void reaped(int status);
  void terminate();
  void finish();
  std::filesystem::path executable_, workingDirectory_;
  std::vector<std::string> arguments_;
  unsigned timeout_;
  Completion completion_;
  Stream output_, error_;
  boost::asio::steady_timer pollTimer_, deadline_, killTimer_, drainTimer_;
  pid_t pid_ = -1;
  int exitCode_ = -1;
  bool exited_ = false, done_ = false, cancelled_ = false, timedOut_ = false;
};
}
