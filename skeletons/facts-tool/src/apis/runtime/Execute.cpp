#include "apis/runtime/Request.h"
#include "apis/operations/Operations.h"
#include "apis/v2/Jobs.h"

namespace facts::apis::runtime {
domain::Result<Json> execute(const domain::Context &context, const Request &request) {
  if (request.operation.starts_with("v2."))
    return v2::executeJob(context, request);
  return domain::resolveFile(context, request.file).and_then([&](const auto &file)
      -> domain::Result<Json> {
    if (request.operation == "extract")
      return operations::extract(context, file, {request.options.value("force", false)});
    if (request.operation == "dependencies") return operations::dependencies(context, file);
    operations::MatchRequest match{request.options.at("query").get<std::string>()};
    if (request.options.contains("traversal"))
      match.traversal = request.options["traversal"].get<std::string>();
    if (request.options.contains("relation_kind"))
      match.relationKind = request.options["relation_kind"].get<std::string>();
    match.captureSource = request.options.value("capture_source", false);
    return operations::match(context, file, match);
  });
}
}
