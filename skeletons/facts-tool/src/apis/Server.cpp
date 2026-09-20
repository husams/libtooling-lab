#include "apis/Server.h"
#include "apis/config/Parse.h"
#include "apis/config/Persistence.h"
#include "apis/daemon/Lifecycle.h"
#include "apis/http/Listener.h"
#include "apis/watch/Watcher.h"
#include <boost/asio/signal_set.hpp>
#include <boost/asio/steady_timer.hpp>
#include <csignal>
#include <iostream>

namespace facts::apis {
namespace {
int serve(Settings settings, const std::vector<std::string> &commands) {
  std::signal(SIGPIPE, SIG_IGN);
  Lifecycle lifecycle(settings);
  auto started = lifecycle.start();
  if (!started) throw std::runtime_error(started.error());
  if (!*started) return 0;
  std::filesystem::current_path(settings.workingDirectory);
  boost::asio::io_context io;
  Queue jobs(io, settings);
  Watcher watcher(io, jobs, settings);
  boost::asio::steady_timer shutdownTimer(io);
  bool stopping = false;
  const auto documents = openApiDocuments(commands, !settings.token.empty());
  Router router{jobs, settings, commands, documents, [&] { return watcher.status(); }, {}};
  Listener listener(io, router);
  router.shutdown = [&] {
    if (stopping) return;
    stopping = true;
    listener.stop();
    watcher.stop();
    jobs.stop();
    // Allow the accepted shutdown response and cancellation callbacks to drain.
    shutdownTimer.expires_after(std::chrono::milliseconds(200));
    shutdownTimer.async_wait([&](auto) { io.stop(); });
  };
  auto port = listener.bind(settings);
  if (!port) throw std::runtime_error("cannot listen: " + port.error());
  settings.port = *port;
  auto watching = watcher.start();
  if (!watching) throw std::runtime_error(watching.error());
  auto saved = saveSettings(settings);
  if (!saved) throw std::runtime_error(saved.error());
  boost::asio::signal_set signals(io, SIGINT, SIGTERM);
  signals.async_wait([&](auto ec, int) { if (!ec) router.shutdown(); });
  listener.accept();
  auto ready = lifecycle.ready(settings.host, settings.port);
  if (!ready) throw std::runtime_error(ready.error());
  std::cout << "facts-tool: server ready at " << settings.host << ':'
            << settings.port << '\n' << std::flush;
  io.run();
  return 0;
}
}
int run(int argc, char **argv, const std::vector<std::string> &commands) {
  auto settings = parseSettings(argc, argv);
  if (!settings) return settings.error();
  try { return serve(std::move(*settings), commands); }
  catch (const std::exception &error) {
    std::cerr << "facts-tool: server error: " << error.what() << '\n';
    return 1;
  }
}
}
