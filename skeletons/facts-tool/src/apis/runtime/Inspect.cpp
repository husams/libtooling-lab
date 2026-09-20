#include "apis/runtime/State.h"
#include <boost/asio/post.hpp>

namespace facts::apis::runtime {
void Service::inspect(const std::string &id, JobReader work, ResourceCompletion completion) {
  const auto found = state_->jobs.find(id);
  if (found == state_->jobs.end()) {
    completion(std::unexpected(domain::Error{404, "job_not_found", "Unknown job"})); return;
  }
  if (state_->queries >= 64) {
    completion(std::unexpected(domain::Error{429, "query_capacity", "Too many resource queries"})); return;
  }
  ++state_->queries;
  const auto document = state_->documents.contains(id) ? state_->documents.at(id) : nullptr;
  boost::asio::post(state_->readers, [state = state_, metadata = found->second, document,
      work = std::move(work), completion = std::move(completion)]() mutable {
    domain::Result<Json> result;
    const Json empty = nullptr;
    try { result = work(metadata, document ? *document : empty); }
    catch (const std::exception &error) {
      result = std::unexpected(domain::Error{500, "result_failed", error.what()});
    }
    boost::asio::post(state->io, [state, completion = std::move(completion), result = std::move(result)]() mutable {
      --state->queries;
      completion(std::move(result));
    });
  });
}
}
