#include "apis/domain/Selection.h"

namespace facts::apis::domain {
Result<Context> resolveContext(const Settings &settings) {
  config::Request request;
  for (std::size_t i = 0; i + 1 < settings.defaults.size(); i += 2) {
    if (settings.defaults[i] == "--conf") request.direct = settings.defaults[i + 1];
    if (settings.defaults[i] == "--config") request.selector = settings.defaults[i + 1];
  }
  try {
    return config::resolve(request)
        .transform([](config::Resolved configuration) {
          return Context{std::move(configuration)};
        })
        .transform_error([](const std::string &message) {
          return Error{500, "server_configuration", message};
        });
  } catch (const std::exception &error) {
    return std::unexpected(Error{500, "server_configuration", error.what()});
  }
}
} // namespace facts::apis::domain
