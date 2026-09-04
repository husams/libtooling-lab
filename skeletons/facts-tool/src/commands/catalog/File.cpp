#include "storage/catalog/File.h"
#include "commands/catalog/Commands.h"
#include "commands/catalog/Run.h"
#include "tooling/CompilationCommandCodec.h"
#include <algorithm>
#include <format>
#include <regex>
#include <span>

namespace facts::commands {
namespace {

catalog::Result<std::string>
displayFiles(const std::vector<catalog::File> &values) {
  if (values.empty())
    return "No files registered\n";
  std::string output =
      "ID\tCOMPONENT\tDIRECTORY\tFILE\tOVERRIDDEN\tINDEXED\tPATH\n";
  for (const auto &value : values) {
    auto path = catalog::filePath(value);
    if (!path)
      return std::unexpected(path.error());
    output += std::format("{}\t{}\t{}\t{}\t{}\t{}\t{}\n", value.id,
                          value.componentName, value.directory, value.name,
                          value.argsOverridden, value.indexed, path->string());
  }
  return output;
}

catalog::Result<std::string> displayFile(const catalog::File &value) {
  return catalog::filePath(value).transform([&](const auto &path) {
    return std::format(
        "ID: {}\nPATH: {}\nCOMPONENT: {}\nDIRECTORY: {}\nFILE: {}\n"
        "DRIVER: {}\nWORKING DIRECTORY: {}\nCOMPILE OPTIONS: {}\n"
        "ARGS OVERRIDDEN: {}\nINDEXED: {}\n",
        value.id, path.string(), value.componentName, value.directory,
        value.name, value.driver, value.workingDirectory, value.compileOptions,
        value.argsOverridden, value.indexed);
  });
}

bool occurrenceAt(const std::vector<std::string> &values,
                  const std::vector<std::string> &sequence,
                  std::size_t position) {
  return !sequence.empty() && position + sequence.size() <= values.size() &&
         std::ranges::equal(
             sequence, std::span(values).subspan(position, sequence.size()));
}

std::vector<std::string>
withoutSequence(const std::vector<std::string> &values,
                const std::vector<std::string> &sequence) {
  std::vector<std::string> result;
  for (std::size_t index = 0; index < values.size();) {
    if (occurrenceAt(values, sequence, index)) {
      index += sequence.size();
      continue;
    }
    result.push_back(values[index++]);
  }
  return result;
}

struct CompileOptionsUpdate {
  std::int64_t id;
  std::vector<std::string> arguments;
};

catalog::Result<std::regex> compilePattern(const std::string &pattern) {
  try {
    return std::regex(pattern, std::regex::ECMAScript);
  } catch (const std::regex_error &error) {
    return std::unexpected("invalid regular expression: " +
                           std::string(error.what()));
  }
}

catalog::Result<std::vector<CompileOptionsUpdate>>
selectedUpdates(const std::vector<catalog::File> &values,
                const std::regex &pattern,
                const std::vector<std::string> &sequence, bool append) {
  std::vector<CompileOptionsUpdate> updates;
  for (const auto &value : values) {
    if (!std::regex_search(catalog::relativeFilePath(value), pattern))
      continue;
    auto decoded = decodeCompileOptions(value.compileOptions);
    if (!decoded)
      return std::unexpected("invalid compile options for " + value.name +
                             ": " + decoded.error());
    auto changed = withoutSequence(*decoded, sequence);
    if (append)
      changed.insert(changed.end(), sequence.begin(), sequence.end());
    updates.push_back({value.id, std::move(changed)});
  }
  if (updates.empty())
    return std::unexpected("regular expression matched no registered files");
  return updates;
}

catalog::Result<std::string>
applyUpdates(catalog::Database &database,
             const std::vector<CompileOptionsUpdate> &updates) {
  for (const auto &update : updates) {
    auto stored = catalog::setFileCompileOptions(
        database, update.id, encodeCompileOptions(update.arguments));
    if (!stored)
      return std::unexpected(stored.error());
  }
  return std::format("Updated {} file(s)\n", updates.size());
}

catalog::Result<std::string> editOptions(catalog::Database &database,
                                         const cli::FileOptions &options,
                                         bool append) {
  return compilePattern(options.match).and_then([&](const auto &pattern) {
    return catalog::files(database).and_then([&](const auto &values) {
      return selectedUpdates(values, pattern, options.arguments, append)
          .and_then([&](const auto &updates) {
            return applyUpdates(database, updates);
          });
    });
  });
}

catalog::Result<std::string> operate(catalog::Database &database,
                                     const cli::FileOptions &options) {
  using Action = cli::FileOptions::Action;
  switch (options.action) {
  case Action::list:
    return catalog::files(database).and_then(displayFiles);
  case Action::show:
    return catalog::file(database, options.path).and_then(displayFile);
  case Action::add:
    return catalog::addFile(database, options.path, options.driver,
                            options.workingDirectory,
                            encodeCompileOptions(options.arguments))
        .transform([] { return std::string{"File added\n"}; });
  case Action::remove:
    return catalog::file(database, options.path)
        .and_then([&](const auto &value) {
          return catalog::removeFile(database, value.id);
        })
        .transform([] { return std::string{"File removed from catalog\n"}; });
  case Action::setOption:
    return editOptions(database, options, true);
  case Action::clearOption:
    return editOptions(database, options, false);
  }
  return std::unexpected("unsupported file action");
}

} // namespace

catalog::Result<int> runFile(const cli::FileOptions &options) {
  using Action = cli::FileOptions::Action;
  const bool writable = options.action == Action::add ||
                        options.action == Action::remove ||
                        options.action == Action::setOption ||
                        options.action == Action::clearOption;
  return runCatalog(options.configuration, writable,
                    [&](auto &database) { return operate(database, options); },
                    false, options.configurationFile);
}

} // namespace facts::commands
