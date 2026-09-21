#pragma once
#include "apis/operations/Operations.h"
#include "tooling/DiagnosticScope.h"
#include "storage/SqliteQuery.h"
#include "apis/operations/Compilation.h"
#include "storage/CloneContext.h"

namespace facts::apis::operations {
inline nlohmann::json diagnosticResults(const DiagnosticScope &scope) {
  auto result = nlohmann::json::array();
  for (const auto &item : scope.messages())
    result.push_back({{"severity", item.severity}, {"message", item.message},
                      {"file", item.file}, {"line", item.line},
                      {"column", item.column}});
  return result;
}
template <typename Work>
Result withDiagnostics(Work work) {
  DiagnosticScope scope;
  auto result = [&]() -> Result {
    try { return work(); }
    catch (const storage::QueryError &error) {
      return std::unexpected(domain::Error{500, "storage_error", error.what()});
    } catch (const std::exception &error) {
      return std::unexpected(domain::Error{500, "operation_failed", error.what()});
    }
  }();
  auto diagnostics = diagnosticResults(scope);
  if (result) (*result)["diagnostics"] = std::move(diagnostics);
  else {
    auto &error = result.error();
    if (!error.details.is_object()) error.details = nlohmann::json::object();
    if (!scope.trace().stage.empty()) error.details["stage"] = scope.trace().stage;
    if (!scope.trace().commands.empty()) {
      auto commands = nlohmann::json::array();
      for (const auto &command : scope.trace().commands)
        commands.push_back({{"source_file", command.source}, {"working_directory", command.directory},
            {"arguments", command.arguments}, {"driver", command.arguments.empty() ? "" : command.arguments.front()}});
      error.details["effective_compilation_commands"] = std::move(commands);
    }
    auto &messages = error.details["diagnostics"];
    if (!messages.is_array()) messages = nlohmann::json::array();
    for (auto &diagnostic : diagnostics) messages.push_back(std::move(diagnostic));
    // Keep context supplied by inner operations, including candidate commands
    // and the compilation database path, when attaching compiler diagnostics.
    for (const auto &diagnostic : messages) {
      const auto severity = diagnostic.value("severity", "");
      if (severity != "error" && severity != "fatal") continue;
      const auto message = diagnostic.value("message", "");
      if (error.message.find(message) != std::string::npos) break;
      const auto file = diagnostic.value("file", "");
      error.message += ": " + (file.empty() ? std::string{} : file + ":" +
          std::to_string(diagnostic.value("line", 0)) + ":" +
          std::to_string(diagnostic.value("column", 0)) + ": ") + message;
      break;
    }
  }
  return result;
}
template <typename Work>
Result withFileContext(const domain::Context &server, const domain::ResolvedFile &file, Work work) {
  ScopedCloneContext clone(file.clone);
  const auto root = file.clone ? std::filesystem::path(file.clone->path)
                               : server.configuration.projectRoot;
  return domain::workspaceContext(server, root).and_then([&](const domain::Context &context) {
    return withDiagnostics([&] { return work(context); })
        .transform_error([&](domain::Error error) {
          return compilationFailure(context, file, std::move(error));
        });
  });
}

}
