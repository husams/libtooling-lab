#include "apis/http/Arguments.h"
#include "apis/http/Router.h"

namespace facts::apis {
Response Router::submit(const Request &request, std::string path) {
  const auto body = Json::parse(request.body(), nullptr, false);
  if (body.is_discarded()) return error(400, "Malformed JSON");
  auto argv = arguments(body, path, commands, settings);
  if (!argv) return error(400, argv.error());
  auto id = jobs.submit(std::move(*argv));
  if (!id) return error(429, "Job queue is full or shutting down");
  auto reply = response(202, *jobs.get(*id));
  reply.set(boost::beast::http::field::location,
            endpoint(generated::Operation::getJob, *id));
  return reply;
}
}
