#include "apis/runtime/State.h"
#include <boost/asio/post.hpp>

namespace facts::apis::runtime {
void State::refresh() {
  if (stopped || refreshPending || !context) return;
  refreshPending = true;
  indexStatus["state"] = "queued";
  indexStatus["pending"] = true;
  enqueue([weak = weak_from_this(), context = *context] {
    const auto self = weak.lock();
    if (!self) return;
    boost::asio::post(self->io, [weak] {
      if (auto self = weak.lock()) self->indexStatus["state"] = "running";
    });
    domain::Result<index::RefreshResult> result;
    try {
      result = domain::factSources(context).and_then([&](const auto &sources) {
        return index::refresh(context.configuration.database, sources,
                              context.configuration.projectRoot)
            .transform_error([](const auto &message) {
              return domain::Error{503, "index_failed", message};
            });
      });
    } catch (const std::exception &error) {
      result = std::unexpected(domain::Error{503, "index_failed", error.what()});
    }
    boost::asio::post(self->io, [weak, result = std::move(result)]() mutable {
      if (auto self = weak.lock()) self->indexed(std::move(result));
    });
  });
}
void State::indexed(domain::Result<index::RefreshResult> result) {
  refreshPending = false;
  indexStatus["pending"] = false;
  indexStatus["state"] = result ? "ready" : "failed";
  indexStatus["error"] = result ? Json(nullptr) : Json(result.error().message);
  if (result) {
    indexReady = true;
    indexStatus["index_revision"] = std::to_string(result->generation);
    indexStatus["files"] = result->sources;
    indexStatus["symbols"] = result->symbols;
    indexStatus["updated_at"] = timestamp();
  }
  log(result ? logging::Level::info : logging::Level::error,
      result ? "index.completed" : "index.failed",
      {{"files", indexStatus["files"]}, {"symbols", indexStatus["symbols"]}});
}
Json encode(const index::Page &page) {
  Json items = Json::array();
  for (const auto &symbol : page.items)
    items.push_back({{"qualified_name", symbol.qualifiedName},
      {"kind", symbol.kind}, {"usr", symbol.usr}, {"file_id", symbol.fileId},
      {"is_definition", symbol.definition},
      {"path", symbol.path}, {"repo", symbol.repository},
      {"clone", symbol.clone}, {"component", symbol.component}});
  return {{"items", items},
          {"next_cursor", page.nextCursor ? Json(*page.nextCursor) : Json(nullptr)}};
}
}
