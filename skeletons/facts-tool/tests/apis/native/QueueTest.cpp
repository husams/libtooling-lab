#include "apis/jobs/Queue.h"
#include <boost/asio/steady_timer.hpp>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>
#include <sys/wait.h>
#include <unistd.h>
using namespace facts::apis;
int main(int argc, char **argv) {
  boost::asio::io_context io;
  Settings settings;
  settings.executable = argc > 1 ? argv[1] : "/usr/bin/python3";
  settings.timeoutSeconds = 0;
  Queue queue(io, settings);
  int completed = 0;
  auto marker = "/tmp/forbidden-job-shell-expansion-" + std::to_string(getpid());
  auto good = queue.submit({"-c", "import sys;print(sys.argv[1]);print('err',file=sys.stderr)", "$(touch " + marker + "); spaces"}, [&](bool success) { assert(success); ++completed; });
  auto bad = queue.submit({"-c", "raise SystemExit(7)"}, [&](bool success) { assert(!success); ++completed; });
  auto output = queue.submit({"-c", "import os;os.write(1,b'x'*(5*1024*1024));os.write(2,b'y'*(5*1024*1024))"});
  auto cancelled = queue.submit({"-c", "raise Exception('must not run')"}, [&](bool success) { assert(!success); ++completed; });
  assert(queue.cancel(*cancelled));
  assert(queue.get(*cancelled)->at("state") == "cancelled");
  bool responsive = false;
  boost::asio::steady_timer tick(io, std::chrono::milliseconds(5));
  tick.async_wait([&](auto error) { assert(!error); responsive = true; });
  io.run();
  assert(completed == 3 && responsive);
  assert(queue.get(*good)->at("state") == "succeeded");
  assert(queue.get(*good)->at("stdout").get<std::string>().find("$(touch") != std::string::npos);
  assert(queue.get(*good)->at("stderr") == "err\n");
  assert(queue.get(*bad)->at("exit_code") == 7);
  auto record = *queue.get(*output);
  assert(record["truncated"] == true);
  assert(record["stdout"].get<std::string>().size() == 4*1024*1024);
  assert(record["stderr"].get<std::string>().size() == 4*1024*1024);
  assert(!queue.list()[0].contains("stdout"));
  assert(access(marker.c_str(), F_OK) != 0);
  assert(!queue.submit({std::string("x\0y",3)}));
  io.restart();
  auto active = queue.submit({"-c", "import signal,time;signal.signal(signal.SIGTERM,signal.SIG_IGN);time.sleep(10)"});
  boost::asio::steady_timer cancel(io, std::chrono::milliseconds(100));
  cancel.async_wait([&](auto) { assert(queue.cancel(*active)); });
  io.run();
  assert(queue.get(*active)->at("state") == "cancelled");
  assert(queue.get(*active)->at("exit_code") == 137);
  io.restart();
  settings.timeoutSeconds = 1;
  Queue timeouts(io, settings);
  auto timed = timeouts.submit({"-c", "import signal,time;signal.signal(signal.SIGTERM,signal.SIG_IGN);time.sleep(10)"});
  io.run();
  assert(timeouts.get(*timed)->at("state") == "failed");
  assert(timeouts.get(*timed)->at("timed_out") == true);
  io.restart();
  auto stopped = queue.submit({"-c", "import time;time.sleep(10)"});
  boost::asio::steady_timer stop(io, std::chrono::milliseconds(50));
  stop.async_wait([&](auto) { queue.stop(); });
  io.run();
  assert(queue.get(*stopped)->at("state") == "cancelled");
  assert(!queue.submit({"-c", "pass"}));
  int status; assert(waitpid(-1, &status, WNOHANG) == -1 && errno == ECHILD);
  io.restart();
  auto destroyed = std::make_unique<Queue>(io, settings);
  assert(destroyed->submit({"-c", "import time;time.sleep(10)"}));
  io.run_for(std::chrono::milliseconds(50));
  destroyed.reset();
  io.restart(); io.run();
  assert(waitpid(-1, &status, WNOHANG) == -1 && errno == ECHILD);
  io.restart();
  settings.executable = "/this-executable-does-not-exist";
  Queue spawnFailure(io, settings);
  auto missing = spawnFailure.submit({"ignored"});
  io.run();
  assert(spawnFailure.get(*missing)->at("state") == "failed");
  std::cout << "queue lifecycle: success, failure, exact argv, output caps, responsiveness, queued/running cancel, timeout, shutdown/reaping PASS\n";
  io.restart();
  settings.executable = "/usr/bin/true";
  Queue retained(io, settings);
  std::string first;
  for (int round=0; round<3; ++round) {
    for (int n=0; n<64; ++n) { auto id=retained.submit({"ignored"}); assert(id); if(first.empty()) first=*id; }
    assert(!retained.submit({"overflow"}));
    io.run(); io.restart();
  }
  assert(retained.list().size() == 128);
  assert(!retained.get(first));
  std::cout << "queue limits: waiting64 retained128 oldest completion eviction PASS\n";
}
