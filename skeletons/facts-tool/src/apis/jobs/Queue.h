#pragma once
#include "apis/config/Settings.h"
#include <boost/asio/io_context.hpp>
#include <nlohmann/json.hpp>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace facts::apis {
using Json = nlohmann::json;
using JobCallback = std::function<void(bool)>;
class Queue {
public:
  Queue(boost::asio::io_context &io, const Settings &settings);
  ~Queue();
  // Call from the owning io_context thread; arguments are used exactly as given.
  // At most 64 waiting jobs and 128 retained records, with 4 MiB per output stream.
  std::optional<std::string> submit(std::vector<std::string> arguments,
                                  JobCallback completion = {});
  Json list() const; // Metadata only; get() includes the completed captured output.
  std::optional<Json> get(const std::string &id) const;
  bool cancel(const std::string &id);
  void stop();
private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
}
