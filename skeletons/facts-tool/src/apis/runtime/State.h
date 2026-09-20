#pragma once
#include "apis/runtime/Service.h"
#include "apis/logging/Logger.h"
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/thread_pool.hpp>
#include <deque>
#include <unordered_map>

namespace facts::apis::runtime {
struct Task {
  std::function<void()> work;
  std::function<bool()> admit;
};
struct State : std::enable_shared_from_this<State> {
  State(boost::asio::io_context &io, Queue &legacy, const Settings &settings,
        logging::Logger *logger);
  void start();
  void stop();
  void enqueue(std::function<void()> work, std::function<bool()> admit = {});
  void pump();
  void refresh();
  void indexed(domain::Result<index::RefreshResult> result);
  domain::Result<Json> submit(Request request);
  void run(std::string id, Request request, domain::Context context);
  void complete(std::string id, bool success, Json error,
                std::shared_ptr<const std::string> payload);
  void search(index::Query query, Completion completion);
  Json list() const;
  void get(const std::string &id, Completion completion);
  domain::Result<void> cancel(const std::string &id);
  void log(logging::Level level, std::string_view event, Json fields = {});
  boost::asio::io_context &io;
  Queue &legacy;
  Settings settings;
  logging::Logger *logger;
  boost::asio::thread_pool worker{1}, readers{2};
  boost::asio::steady_timer retry;
  std::optional<domain::Context> context;
  std::deque<Task> work;
  std::unordered_map<std::string, Json> jobs;
  std::unordered_map<std::string, std::shared_ptr<const std::string>> payloads;
  std::deque<std::string> order;
  Json indexStatus{{"state", "queued"}, {"pending", true}, {"files", 0},
                   {"symbols", 0}, {"error", nullptr}, {"updated_at", nullptr}};
  std::uint64_t nextId = 1;
  unsigned queries = 0;
  bool active = false, stopped = false, indexReady = false;
  bool refreshPending = false;
};
std::int64_t timestamp();
domain::Result<domain::Context> initialize(const Settings &settings);
Json encode(const index::Page &page);
Json encodeError(const domain::Error &error);
std::string serialize(const Json &value);
}
