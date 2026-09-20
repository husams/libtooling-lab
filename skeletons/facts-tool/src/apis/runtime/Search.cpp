#include "apis/runtime/State.h"
#include <boost/asio/post.hpp>

namespace facts::apis::runtime {
void State::search(index::Query query, Completion completion) {
  if (stopped || !context || !indexReady) {
    completion(std::unexpected(domain::Error{
        503, "index_not_ready", "The global symbol index is not ready; inspect /v1/index"}));
    return;
  }
  if (queries >= 64) {
    completion(std::unexpected(domain::Error{429, "query_capacity", "Too many symbol queries"}));
    return;
  }
  ++queries;
  boost::asio::post(readers, [weak = weak_from_this(), query = std::move(query),
      project = context->configuration.database, completion = std::move(completion)]() mutable {
    domain::Result<std::string> result;
    try {
      result = index::search(project, query).transform(encode).transform(serialize)
          .transform_error([](const auto &message) {
            const auto cursor = message.find("cursor") != std::string::npos;
            return domain::Error{cursor ? 409U : 503U,
                                cursor ? "invalid_cursor" : "index_unavailable", message};
          });
    } catch (const std::exception &error) {
      result = std::unexpected(domain::Error{503, "index_unavailable", error.what()});
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
