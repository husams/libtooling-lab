#include "apis/logging/Logger.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cerrno>
#include <fcntl.h>
#include <iostream>
#include <set>
#include <sstream>
#include <thread>
#include <unistd.h>

using namespace facts::apis::logging;
int main() {
  int pipeDescriptors[2];
  assert(::pipe(pipeDescriptors) == 0);
  assert(::fcntl(pipeDescriptors[1], F_SETPIPE_SZ, 4096) >= 4096);
  const int previousStderr = ::dup(STDERR_FILENO);
  assert(previousStderr >= 0);
  assert(::dup2(pipeDescriptors[1], STDERR_FILENO) == STDERR_FILENO);
  auto logger = std::make_unique<Logger>(Options{{}, Level::trace});
  assert(::dup2(previousStderr, STDERR_FILENO) == STDERR_FILENO);
  ::close(previousStderr);
  ::close(pipeDescriptors[1]);
  constexpr int submitted = 10000;
  boost::asio::io_context io;
  bool responsive = false;
  boost::asio::post(io, [&] {
    for (int index = 0; index < submitted; ++index)
      logger->write(Level::info, "test.accepted",
                    {{"sequence", index}, {"payload", std::string(4096, 'x')}});
    boost::asio::post(io, [&] { responsive = true; });
  });
  // No reader exists yet: sink writes block, but producer callbacks must finish.
  io.run();
  assert(responsive);
  std::string output;
  std::thread reader([&] {
    char buffer[8192];
    for (;;) {
      const auto count = ::read(pipeDescriptors[0], buffer, sizeof(buffer));
      if (count < 0 && errno == EINTR) continue;
      if (count == 0) break;
      assert(count > 0);
      output.append(buffer, static_cast<std::size_t>(count));
    }
  });
  logger.reset(); // Drains all accepted entries before closing its descriptor.
  reader.join();
  ::close(pipeDescriptors[0]);
  std::set<int> sequences;
  int dropped = 0;
  std::istringstream lines(output);
  for (std::string line; std::getline(lines, line);) {
    const auto record = nlohmann::json::parse(line);
    assert(record.at("timestamp").is_number_integer());
    if (record.at("event") == "logger.dropped")
      dropped += record.at("fields").at("records").get<int>();
    else {
      assert(record.at("event") == "test.accepted");
      assert(sequences.insert(record.at("fields").at("sequence").get<int>()).second);
    }
  }
  assert(dropped > 0 && !sequences.empty());
  assert(sequences.size() + dropped == submitted);
  std::cout << "blocked sink: responsive callbacks, bounded queue, exact drop accounting and drain PASS\n";
}
