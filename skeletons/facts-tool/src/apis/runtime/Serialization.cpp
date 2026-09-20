#include "apis/runtime/State.h"
#include <boost/asio/post.hpp>

namespace facts::apis::runtime {
std::string serialize(const Json &value) {
  return value.dump(-1, ' ', false, Json::error_handler_t::replace);
}
Json encodeError(const domain::Error &error) {
  Json value{{"code", error.code}, {"message", error.message}};
  if (!error.details.is_null()) value["details"] = error.details;
  return value;
}
namespace {
std::string snapshot(Json metadata, const std::shared_ptr<const std::string> &payload) {
  if (!payload) return serialize(metadata);
  metadata.erase("result");
  metadata.erase("error");
  auto result = serialize(metadata);
  result.pop_back();
  result += ',';
  result.append(*payload, 1, payload->size() - 1);
  return result;
}
}
void State::get(const std::string &id, Completion completion) {
  const auto found = jobs.find(id);
  if (found == jobs.end() || found->second.at("operation").get<std::string>().starts_with("v2.")) {
    completion(std::unexpected(domain::Error{404, "job_not_found", "Unknown job"}));
    return;
  }
  if (queries >= 64) {
    completion(std::unexpected(domain::Error{429, "query_capacity", "Too many resource queries"}));
    return;
  }
  ++queries;
  const auto payload = payloads.contains(id) ? payloads.at(id) : nullptr;
  boost::asio::post(readers, [weak = weak_from_this(), metadata = found->second,
      payload, completion = std::move(completion)]() mutable {
    domain::Result<std::string> result;
    try { result = snapshot(std::move(metadata), payload); }
    catch (const std::exception &error) {
      result = std::unexpected(domain::Error{500, "serialization_failed", error.what()});
    }
    if (auto self = weak.lock()) boost::asio::post(self->io,
        [weak, completion = std::move(completion), result = std::move(result)]() mutable {
      if (auto self = weak.lock()) {
        --self->queries;
        completion(std::move(result));
      }
    });
  });
}
}
