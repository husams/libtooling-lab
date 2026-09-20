#include "apis/runtime/State.h"
#include "storage/catalog/Database.h"
#include <boost/asio/post.hpp>

namespace facts::apis::runtime {
domain::Result<domain::Context> initialize(const Settings &settings) {
  try {
  return domain::resolveContext(settings).and_then([](domain::Context context)
      -> domain::Result<domain::Context> {
    std::error_code error;
    const auto &path = context.configuration.database;
    if (!path.parent_path().empty())
      std::filesystem::create_directories(path.parent_path(), error);
    if (error) return std::unexpected(domain::Error{
        503, "project_unavailable", error.message()});
    return catalog::open(path.string(), true, true)
        .transform_error([](const auto &message) {
          return domain::Error{503, "project_unavailable", message};
        }).transform([&](auto) { return std::move(context); });
  });
  } catch (const std::exception &error) {
    return std::unexpected(domain::Error{503, "project_unavailable", error.what()});
  }
}
void State::start() {
  indexStatus["state"] = "running";
  enqueue([weak = weak_from_this(), settings = settings] {
    auto context = initialize(settings);
    std::optional<index::RefreshResult> snapshot;
    if (context) {
      const auto published = index::published(context->configuration.database);
      if (published) snapshot = *published;
    }
    if (auto self = weak.lock()) boost::asio::post(self->io,
        [weak, context = std::move(context), snapshot]() mutable {
      const auto self = weak.lock();
      if (!self || self->stopped) return;
      if (!context) {
        self->indexed(std::unexpected(context.error()));
        return;
      }
      self->context = std::move(*context);
      if (snapshot) {
        self->indexReady = true;
        self->indexStatus["index_revision"] = std::to_string(snapshot->generation);
        self->indexStatus["files"] = snapshot->sources;
        self->indexStatus["symbols"] = snapshot->symbols;
      }
      self->refresh();
    });
  });
}
}
