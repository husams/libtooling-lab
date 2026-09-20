#pragma once
#include "apis/config/Settings.h"
#include <expected>

namespace facts::apis {
class Lifecycle {
public:
  explicit Lifecycle(const Settings &settings) : settings_(settings) {}
  ~Lifecycle();
  Lifecycle(const Lifecycle &) = delete;
  Lifecycle &operator=(const Lifecycle &) = delete;
  std::expected<bool, std::string> start();
  std::expected<void, std::string> ready(const std::string &host, std::uint16_t port);
private:
  std::expected<bool, std::string> detach();
  Settings settings_;
  int lock_ = -1;
  int readiness_ = -1;
  bool owner_ = false;
};
}
