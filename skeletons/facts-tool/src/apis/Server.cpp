#include "apis/Server.h"
#include "apis/config/Parse.h"
#include "apis/config/Persistence.h"
#include "apis/daemon/Lifecycle.h"
#include "apis/http/Listener.h"
#include "apis/logging/Logger.h"
#include "apis/logging/Record.h"
#include "apis/watch/Watcher.h"
#include "apis/runtime/Service.h"
#include "apis/runtime/Drain.h"
#include "apis/v2/http/Dispatch.h"
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
  logging::Logger logger(settings.logging);
  std::string_view stage = "working_directory";
  try {
    std::filesystem::current_path(settings.workingDirectory);
    boost::asio::io_context io;
    Queue jobs(io, settings, &logger);
    runtime::Service resources(io, jobs, settings, &logger);
    Watcher watcher(io, jobs, settings, &logger);
    boost::asio::steady_timer shutdownTimer(io);
    bool stopping = false;
    const auto documents = openApiDocuments(commands, !settings.token.empty());
    Router router{jobs, settings, commands, documents,
                  [&] { return watcher.status(); }, {}, &logger, &resources};
    router.reconcile = [&] { watcher.reconcile(); };
    router.updateWatch = [&](const Json &body, bool replace) -> domain::Result<Json> {
      return v2::http::patchSettings(settings, body, replace).and_then([&](const Settings &next) -> domain::Result<Json> {
        auto saved = saveSettings(next);
        if (!saved) return std::unexpected(domain::Error{500, "settings_failed", saved.error()});
        auto changed = watcher.reconfigure(next);
        if (!changed) {
          (void)saveSettings(settings);
          return std::unexpected(domain::Error{500, "watcher_failed", changed.error()});
        }
        settings = next;
        resources.updateSettings(settings);
        return v2::http::watchSettings(settings);
      });
    };
    Listener listener(io, router);
    router.shutdown = [&] {
      if (stopping) return;
      stopping = true;
      logger.write(logging::Level::info, "server.stopping");
      listener.stop();
      watcher.stop();
      resources.stop();
      jobs.stop();
      // Allow the accepted shutdown response and cancellation callbacks to drain.
      runtime::drain(io, resources, shutdownTimer);
    };
    stage = "listener";
    auto port = listener.bind(settings);
    if (!port) throw std::runtime_error("cannot listen: " + port.error());
    settings.port = *port;
    stage = "watcher";
    auto watching = watcher.start();
    if (!watching) throw std::runtime_error(watching.error());
    stage = "configuration";
    auto saved = saveSettings(settings);
    if (!saved) throw std::runtime_error(saved.error());
    boost::asio::signal_set signals(io, SIGINT, SIGTERM);
    signals.async_wait([&](auto ec, int) { if (!ec) router.shutdown(); });
    listener.accept();
    resources.start();
    stage = "readiness";
    auto ready = lifecycle.ready(settings.host, settings.port);
    if (!ready) throw std::runtime_error(ready.error());
    logger.write(logging::Level::info, "server.ready",
        {{"host", settings.host}, {"port", settings.port}, {"daemon", settings.daemon}});
    if (!settings.daemon)
      std::cout << "facts-tool: server ready at " << settings.host << ':'
                << settings.port << '\n' << std::flush;
    stage = "event_loop";
    io.run();
    logger.write(logging::Level::info, "server.stopped");
    return 0;
  } catch (const std::exception &error) {
    const Json details{{"stage", stage}, {"reason", error.what()}};
    std::cerr << logging::record(logging::Level::error, "server.failed", details);
    if (!settings.daemon && !settings.logging.file.empty())
      logger.write(logging::Level::error, "server.failed", details);
    return 1;
  }
}
}
int run(int argc, char **argv, const std::vector<std::string> &commands) {
  auto settings = parseSettings(argc, argv);
  if (!settings) return settings.error();
  try { return serve(std::move(*settings), commands); }
  catch (const std::exception &error) {
    std::cerr << logging::record(logging::Level::error, "server.failed",
                                {{"stage", "startup"}, {"reason", error.what()}});
    return 1;
  }
}
}
