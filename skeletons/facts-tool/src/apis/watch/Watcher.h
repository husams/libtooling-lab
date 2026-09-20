#pragma once
#include "apis/config/Settings.h"
#include "apis/jobs/Queue.h"
#include <expected>
#include <memory>

namespace facts::apis {
class Watcher {
public:
  Watcher(boost::asio::io_context &io, Queue &queue, const Settings &settings);
  ~Watcher();
  std::expected<void, std::string> start();
  void stop();
  Json status() const;
private:
  struct Impl;
  std::shared_ptr<Impl> impl_;
};
}
