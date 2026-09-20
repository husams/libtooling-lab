#include "apis/domain/Candidates.h"
#include <set>

namespace facts::apis::domain {
Result<std::vector<std::filesystem::path>> factSources(const Context &context) {
  return catalog::open(context.configuration.database.string(), false)
      .and_then([](catalog::Database database) { return catalog::files(database); })
      .transform_error([](const std::string &message) {
        return Error{503, "project_unavailable", message};
      })
      .and_then([&](std::vector<catalog::File> files)
                    -> Result<std::vector<std::filesystem::path>> {
        std::set<std::filesystem::path> sources;
        for (auto &file : files) {
          auto source = catalog::filePath(file);
          if (!source)
            return std::unexpected(Error{500, "file_configuration", source.error()});
          auto destination = detail::factsPath(context, {std::move(file), {}, true},
                                                *source);
          if (!destination) return std::unexpected(destination.error());
          sources.insert(destination->lexically_normal());
        }
        return std::vector<std::filesystem::path>{sources.begin(), sources.end()};
      });
}
} // namespace facts::apis::domain
