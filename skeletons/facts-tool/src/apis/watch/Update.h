#pragma once
#include "apis/watch/Scan.h"
#include "apis/watch/plan/Plan.h"
#include <memory>

namespace facts::apis::watch {
struct Event {
  std::filesystem::path path;
  bool directory = false;
  bool control = false;
};
struct Update {
  std::shared_ptr<const Scan> snapshot;
  Plan plan;
  std::size_t events = 0;
  bool refresh = false;
};
std::expected<std::size_t, std::string> acceptedEvents(
    const Settings &, const Catalog &, const std::vector<Event> &);
std::expected<Update, std::string> update(const Settings &,
    std::shared_ptr<const Scan>, std::vector<Event>, bool,
    const std::atomic_bool &);
}
