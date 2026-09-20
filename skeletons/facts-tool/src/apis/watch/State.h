#pragma once
#include "apis/watch/Watcher.h"
#include "apis/watch/Paths.h"
#include "apis/watch/Scan.h"
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/thread_pool.hpp>
#include <array>
#include <cstddef>
#include <deque>
#include <map>
#include <set>
#ifdef __linux__
#include <boost/asio/posix/stream_descriptor.hpp>
#endif

namespace facts::apis {
struct Watcher::Impl : std::enable_shared_from_this<Impl> {
  Impl(boost::asio::io_context &io, Queue &queue, const Settings &settings);
  std::expected<void, std::string> start();
  void stop();
  Json status() const;
  void changed();
  void refresh();
  void importNext();
  void extract();
  void finished(bool success);
  std::vector<std::string> arguments(std::string command,
                                    std::vector<std::string> values) const;
  void prepareImports();
#ifdef __linux__
  void scan();
  void scanned(std::expected<watch::Scan, std::string> result);
  void retry();
  std::expected<void, std::string> applyScan(const watch::Scan &result);
  void read();
  void consume(std::size_t bytes);
  void event(int descriptor, unsigned mask, const std::string &name);
  boost::asio::posix::stream_descriptor descriptor;
  alignas(std::max_align_t) std::array<char, 65536> buffer{};
  std::map<int, std::filesystem::path> watches;
#endif
  Queue &queue;
  boost::asio::io_context &io;
  Settings settings;
  boost::asio::steady_timer debounce;
  boost::asio::steady_timer recovery;
  boost::asio::thread_pool scanner{1};
  std::atomic_bool cancelled{false};
  std::set<std::filesystem::path> compilationDirectories;
  std::deque<std::vector<std::string>> imports;
  std::vector<std::string> latestJobs;
  std::string error;
  std::uint64_t events = 0;
  std::uint64_t cycles = 0;
  std::uint64_t failures = 0;
  std::uint64_t overflows = 0;
  bool running = false;
  bool active = false;
  bool dirty = false;
  bool needsScan = false;
  bool scanning = false;
  bool ready = false;
};
}
