#pragma once
#include "apis/config/Settings.h"
#include "apis/jobs/Queue.h"
#include <expected>
#include <memory>

namespace facts::apis {
namespace logging { class Logger; }
class Watcher {
public:
  Watcher(boost::asio::io_context &io, Queue &queue, const Settings &settings,
          logging::Logger *logger = nullptr);
  ~Watcher();
  std::expected<void, std::string> start();
  void stop();
  std::expected<void, std::string> reconfigure(const Settings &);
  void reconcile();
  Json status() const;
private:
  struct Impl;
  std::shared_ptr<Impl> impl_;
};
}
