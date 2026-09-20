#include "apis/http/OpenApi.h"

namespace facts::apis {
namespace {
Json ref(const std::string &name) { return {{"$ref", "#/components/schemas/" + name}}; }
Json array(Json item) { return {{"type", "array"}, {"items", std::move(item)}}; }
Json object(Json properties) { return {{"type", "object"}, {"properties", std::move(properties)}}; }
Json operation(const std::string &summary, unsigned code, Json schema) {
  return {{"summary", summary}, {"responses", {
      {std::to_string(code), {{"description", "Success"}, {"content", {
          {"application/json", {{"schema", std::move(schema)}}}}}}},
      {"default", {{"$ref", "#/components/responses/Error"}}}}}};
}
Json submission(const std::string &summary, const std::string &request) {
  auto result = operation(summary, 202, ref("Job"));
  result["requestBody"] = {{"required", true}, {"content", {
      {"application/json", {{"schema", ref(request)}}}}}};
  result["responses"]["202"]["headers"]["Location"] = {
      {"description", "Job polling endpoint"}, {"schema", {{"type", "string"}}}};
  return result;
}
Json schemas() {
  const Json text{{"type", "string"}}, boolean{{"type", "boolean"}};
  const Json timestamp{{"type", "integer"}, {"format", "int64"},
                       {"description", "Unix epoch milliseconds"}};
  auto optionalTimestamp = timestamp;
  optionalTimestamp["type"] = {"integer", "null"};
  auto arguments = array({{"type", "string"}, {"description", "One CLI argument; no shell expansion"}});
  arguments["maxItems"] = 4096;
  auto request = object({{"arguments", arguments}});
  request["required"] = {"arguments"};
  request["additionalProperties"] = false;
  auto job = object({{"id", text}, {"arguments", array(text)}, {"state", {
      {"type", "string"}, {"enum", {"queued", "running", "succeeded", "failed", "cancelled"}}}},
      {"exit_code", {{"type", {"integer", "null"}}}}, {"stdout", text}, {"stderr", text},
      {"truncated", boolean}, {"timed_out", boolean}, {"created_at", timestamp},
      {"started_at", optionalTimestamp}, {"finished_at", optionalTimestamp}});
  job["required"] = {"id", "state", "arguments", "exit_code", "truncated", "timed_out", "created_at"};
  job["description"] = "Job output is populated on completion and omitted from list responses. "
      "Each output stream is capped at 4 MiB; truncated reports overflow. IDs are server-local.";
  auto watch = object({{"directories", array(text)}, {"latest_jobs", array(text)},
      {"backend", text}, {"last_error", text}, {"import_mode", text}, {"notice", text}});
  for (const auto *field : {"enabled", "running", "active", "pending", "scanning", "ready"})
    watch["properties"][field] = boolean;
  for (const auto *field : {"events", "cycles", "failures", "overflows", "watched_directories"})
    watch["properties"][field] = {{"type", "integer"}, {"minimum", 0}};
  auto generic = request;
  generic["properties"]["arguments"]["minItems"] = 1;
  return {{"Arguments", request}, {"JobRequest", generic}, {"Job", job},
          {"WatchStatus", watch}, {"Error", object({{"error", text}})}};
}
}
Json openApi(const std::vector<std::string> &commands, bool authenticated) {
  Json document{{"openapi", "3.1.0"}, {"info", {{"title", "facts-tool REST API"},
      {"version", "1.0.0"}, {"description", "Asynchronous CLI jobs with filesystem reindexing. "
      "Pass argument arrays, then poll the accepted job. Browser-origin requests are rejected."}}},
      {"servers", Json::array({{{"url", "/"}}})}, {"paths", Json::object()}};
  document["components"]["schemas"] = schemas();
  document["components"]["responses"]["Error"] = {{"description", "Request rejected: "
      "400 invalid input, 401 authentication, 403 origin or Host, 404 unknown resource, "
      "405 method, 413 body too large, or 429 queue full"},
      {"content", {{"application/json", {{"schema", ref("Error")}}}}}};
  auto &paths = document["paths"];
  const auto status = object({{"status", {{"type", "string"}}}});
  paths["/health"]["get"] = operation("Check server health", 200, status);
  paths["/openapi.json"]["get"] = operation("Read this OpenAPI schema", 200, {{"type", "object"}});
  const auto command = object({{"path", {{"type", "string"}}}, {"endpoint", {{"type", "string"}}}});
  paths["/v1/commands"]["get"] = operation("List CLI command endpoints", 200,
      object({{"commands", array(command)}}));
  paths["/v1/jobs"]["get"] = operation("List retained job metadata", 200,
      object({{"jobs", array(ref("Job"))}}));
  paths["/v1/jobs"]["post"] = submission("Submit a full CLI argument array", "JobRequest");
  paths["/v1/jobs/{id}"]["parameters"] = Json::array({{{"name", "id"},
      {"in", "path"}, {"required", true}, {"schema", {{"type", "string"}}}}});
  paths["/v1/jobs/{id}"]["get"] = operation("Read job state and captured output", 200, ref("Job"));
  paths["/v1/jobs/{id}"]["delete"] = operation("Request cancellation; poll for final state", 200, ref("Job"));
  paths["/v1/watch"]["get"] = operation("Read filesystem monitor status", 200, ref("WatchStatus"));
  paths["/v1/shutdown"]["post"] = operation("Stop the server and cancel unfinished jobs", 202, status);
  for (const auto &commandPath : commands)
    paths["/v1/commands/" + commandPath]["post"] = submission("Run " + commandPath, "Arguments");
  if (authenticated) {
    document["components"]["securitySchemes"]["bearerAuth"] = {{"type", "http"}, {"scheme", "bearer"}};
    document["security"] = Json::array({{{"bearerAuth", Json::array()}}});
  }
  return document;
}
}
