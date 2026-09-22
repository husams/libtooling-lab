#include "apis/runtime/State.h"
#include <boost/asio/post.hpp>
#include <cstdlib>

namespace facts::apis::runtime {
namespace {
std::string logText(std::string value, std::size_t bytes) {
  // Bound serialized size, since quotes/control characters can expand sixfold.
  while (Json(value).dump(-1, ' ', false, Json::error_handler_t::replace).size() > bytes)
    value = value.substr(0, value.size() / 2) + "...";
  return value;
}
void enrichFailure(domain::Error &error, const domain::Context &context, const Request &request) {
  auto &details = error.details;
  if (!details.is_object()) details = Json::object();
  details["operation"] = request.operation;
  details["request"] = request.options;
  details["server_working_directory"] = request.settings.workingDirectory.string();
  details["project_database"] = context.configuration.database.string();
  if (!details.contains("stage")) details["stage"] = request.operation;
  if (!details.contains("expected")) details["expected"] = request.operation == "v2.index"
      ? "Readable facts databases with a valid facts-tool schema for registered files"
      : "A valid request and accessible registered workspace with the required build inputs";
  if (!details.contains("action")) details["action"] =
      "Correct the reported error and resubmit this operation with retry_of set to this job ID";
  Json environment = Json::object();
  for (const auto *key : {"PATH", "CPATH", "CPLUS_INCLUDE_PATH", "C_INCLUDE_PATH",
                          "SDKROOT", "MACOSX_DEPLOYMENT_TARGET", "FACTS_TOOL_CONFIG"})
    if (const auto *value = std::getenv(key)) environment[key] = value;
  details["environment"] = std::move(environment);
}
void logFailure(State &state, const std::string &id, const Request &request,
            const domain::Error &error, const std::string &fileId = {}) {
  // Log the failure context and each bounded compiler diagnostic separately
  // so a large compiler report does not discard the entire log record.
  Json fields{{"job_id", id}, {"operation", request.operation},
      {"file_id", fileId}, {"code", error.code}, {"message", logText(error.message, 4096)}};
  const auto &details = error.details;
  if (details.is_object()) {
    for (const auto *key : {"path", "repository", "clone_path", "project_root", "stage", "expected", "action"})
      if (details.contains(key) && details.at(key).is_string())
        fields[key] = logText(details.at(key).get<std::string>(), 1024);
    if (details.contains("compilation_commands")) {
      Json directories = Json::array();
      for (const auto &command : details.at("compilation_commands")) {
        if (directories.size() == 8) break;
        directories.push_back(logText(command.value("working_directory", ""), 512));
      }
      fields["working_directories"] = std::move(directories);
    }
    if (details.contains("diagnostics"))
      for (auto diagnostic : details.at("diagnostics")) {
        diagnostic["job_id"] = id;
        diagnostic["file_id"] = fileId;
        for (const auto *key : {"message", "file"})
          if (diagnostic.contains(key) && diagnostic.at(key).is_string())
            diagnostic[key] = logText(diagnostic.at(key).get<std::string>(), 4096);
        state.log(logging::Level::error, "job.diagnostic", std::move(diagnostic));
      }
  }
  state.log(logging::Level::error, fileId.empty() ? "job.failed" : "job.file_failed", std::move(fields));
  // Preserve full context in bounded records, including every compiler option.
  // Parts are JSON string fragments in order, not silently truncated values.
  if (details.is_object()) for (const auto &[key, value] : details.items()) {
    if (key == "diagnostics") continue;
    const auto serialized = value.dump(-1, ' ', true, Json::error_handler_t::replace);
    constexpr std::size_t partSize = 1024;
    for (std::size_t offset = 0; offset < serialized.size(); offset += partSize)
      state.log(logging::Level::error, "job.context", {{"job_id", id}, {"file_id", fileId}, {"key", key},
          {"part", offset / partSize}, {"parts", (serialized.size() + partSize - 1) / partSize},
          {"value", serialized.substr(offset, partSize)}});
  }
}

}
void State::run(std::string id, Request request, domain::Context context) {
  if (!jobs.contains(id) || jobs.at(id)["state"] == "cancelled") return;
  enqueue([weak = weak_from_this(), id, request = std::move(request), context]() mutable {
    const auto self = weak.lock();
    if (!self) return;
    request.reportFileFailure = [&](domain::Error &error, const std::string &fileId) {
      enrichFailure(error, context, request);
      if (request.options.value("continue_on_error", false))
        logFailure(*self, id, request, error, fileId);
    };
    domain::Result<Json> result;
    try { result = execute(context, request); }
    catch (const std::exception &error) {
      result = std::unexpected(domain::Error{500, "operation_failed", error.what()});
    }
    Json partial = nullptr;
    if (!result && result.error().details.is_object() &&
        result.error().details.contains("partial_result")) {
      partial = std::move(result.error().details["partial_result"]);
      result.error().details.erase("partial_result");
    }
    if (request.operation == "v2.extract" || request.operation == "v2.match" ||
        request.operation == "v2.import" || request.operation == "v2.dependencies" ||
        request.operation == "v2.index") {
      try {
      auto published = domain::factSources(context).and_then([&](const auto &sources) {
        return index::refresh(context.configuration.database, sources,
            context.configuration.projectRoot).transform_error([](const auto &message) {
          return domain::Error{503, "index_failed", message};
        });
      });
      if (published) {
        if (partial.is_object()) partial["index_revision"] = std::to_string(published->generation);
        if (result) {
          (*result)["index_revision"] = std::to_string(published->generation);
          if (request.operation == "v2.index") {
            (*result)["sources_processed"] = published->processedSources;
            (*result)["sources_skipped"] = published->skippedSources;
            (*result)["sources_removed"] = published->removedSources;
            (*result)["sources_missing"] = published->missingSources;
            (*result)["symbol_count"] = published->symbols;
          }
        }
        boost::asio::post(self->io, [weak, published = std::move(published)]() mutable {
          if (auto state = weak.lock()) state->indexed(std::move(published));
        });
      } else if (result) {
        if (result->contains("files_selected")) partial = std::move(*result);
        result = std::unexpected(published.error());
      }
      } catch (const std::exception &error) {
        if (result) {
          if (result->contains("files_selected")) partial = std::move(*result);
          result = std::unexpected(domain::Error{503, "index_failed", error.what()});
        }
      }
    }
    if (result && request.operation == "v2.scan" && result->contains("warnings"))
      for (auto warning : result->at("warnings")) {
        warning["job_id"] = id;
        self->log(logging::Level::warning, "scan.warning", std::move(warning));
      }
    if (request.operation == "v2.import") {
      const auto &details = result ? *result : result.error().details;
      if (details.is_object() && details.contains("diagnostics"))
        for (auto diagnostic : details.at("diagnostics"))
          if (diagnostic.value("severity", "") == "warning") {
            diagnostic["job_id"] = id;
            self->log(logging::Level::warning, "import.warning", std::move(diagnostic));
          }
    }
    if (!result) enrichFailure(result.error(), context, request);
    const bool success = result.has_value();
    Json failure = result ? Json(nullptr) : encodeError(result.error());
    if (!result) logFailure(*self, id, request, result.error());
    Json document{{"result", result ? std::move(*result) : std::move(partial)},
                  {"error", std::move(failure)}};
    const bool nativeV2 = request.operation.starts_with("v2.");
    auto payload = nativeV2 ? nullptr : std::make_shared<const std::string>(serialize(document));
    auto retained = nativeV2 ? std::make_shared<const Json>(std::move(document)) : nullptr;
    Json summary = success ? Json(nullptr) : encodeError(result.error());
    boost::asio::post(self->io, [weak, id, success, error = std::move(summary),
                                payload = std::move(payload), retained = std::move(retained)]() mutable {
      if (auto self = weak.lock())
        self->complete(id, success, std::move(error), std::move(payload), std::move(retained));
    });
  }, [weak = weak_from_this(), id] {
    const auto self = weak.lock();
    if (!self || self->stopped || !self->jobs.contains(id) ||
        self->jobs.at(id)["state"] == "cancelled") return false;
    self->jobs.at(id)["state"] = "running";
    self->jobs.at(id)["started_at"] = timestamp();
    self->log(logging::Level::info, "job.started", {{"job_id", id}});
    return true;
  });
}
void State::complete(std::string id, bool success, Json error,
                     std::shared_ptr<const std::string> payload,
                     std::shared_ptr<const Json> document) {
  auto &job = jobs.at(id);
  const bool cancelled = !success && error.is_object() &&
      error.value("code", "") == "cancelled";
  job["state"] = success ? "succeeded" : cancelled ? "cancelled" : "failed";
  job["finished_at"] = timestamp();
  job["error"] = std::move(error);
  payloads[id] = std::move(payload);
  if (document) documents[id] = std::move(document);
  Json outcome{{"job_id", id}, {"state", job["state"]}};
  if (documents.contains(id) && documents.at(id)->at("result").is_object())
    for (const auto &[key, value] : documents.at(id)->at("result").items())
      if (!value.is_array()) outcome[key] = value;
  log(success ? logging::Level::info : logging::Level::error,
      "job.completed", std::move(outcome));
  if (success && !stopped && !job.at("operation").get<std::string>().starts_with("v2.")) refresh();
}
}
