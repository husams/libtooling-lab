#include "apis/runtime/State.h"
#include <boost/asio/post.hpp>

namespace facts::apis::runtime {
void Service::read(ResourceWork work, ResourceCompletion completion) {
  if (state_->stopped || !state_->context) {
    completion(std::unexpected(domain::Error{503, "service_not_ready", "Server initialization is incomplete"}));
    return;
  }
  if (state_->queries >= 64) {
    completion(std::unexpected(domain::Error{429, "query_capacity", "Too many resource queries"}));
    return;
  }
  ++state_->queries;
  boost::asio::post(state_->readers, [state = state_, context = *state_->context,
      work = std::move(work), completion = std::move(completion)]() mutable {
    domain::Result<Json> result;
    try { result = work(context); }
    catch (const std::exception &error) {
      result = std::unexpected(domain::Error{500, "operation_failed", error.what()});
    }
    boost::asio::post(state->io, [state, completion = std::move(completion),
                               result = std::move(result)]() mutable {
      --state->queries;
      completion(std::move(result));
    });
  });
}
}
