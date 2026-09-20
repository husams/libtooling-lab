#include "apis/domain/Candidates.h"

namespace facts::apis::domain::detail {
namespace {
Candidate rowCandidate(const storage::Row &row) {
  Candidate result;
  auto &file = result.file;
  file.id = row.integer(0);
  file.component = {row.integer(1), row.string(2), row.string(3), row.string(4),
                    row.get<std::optional<std::string>>(5),
                    row.get<std::optional<std::int64_t>>(6)};
  file.directory = row.string(7);
  file.name = row.string(8);
  file.factsDb = row.string(9);
  result.repository = row.string(10);
  if (!row.isNull(11))
    file.clone = ProjectClone{row.integer(11), row.integer(12), row.string(13),
                             row.string(14)};
  result.active = row.isNull(11) || row.integer(15) != 0;
  return result;
}
}
Result<std::vector<Candidate>> candidates(const Context &context,
                                         const FileSelector &selector) {
  return catalog::open(context.configuration.database.string(), false)
      .and_then([&](catalog::Database database) {
        return catalog::query(database,
            "SELECT f.id,c.id,c.name,c.path,c.kind,c.version,c.repository_id,"
            "d.path,f.name,coalesce(f.facts_db,''),coalesce(r.name,''),"
            "cl.id,cl.repository_id,cl.path,coalesce(cl.label,''),"
            "coalesce(r.active_clone_id=cl.id,0) FROM file f "
            "JOIN directory d ON d.id=f.directory_id "
            "JOIN component c ON c.id=d.component_id "
            "LEFT JOIN repository r ON r.id=c.repository_id "
            "LEFT JOIN clone cl ON cl.repository_id=r.id "
            "WHERE f.name=?1 AND (?2='' OR r.name=?2) "
            "AND (?3='' OR c.name=?3) "
            "AND (?4='' OR cl.label=?4 OR cl.path=?4 OR CAST(cl.id AS TEXT)=?4) "
            "ORDER BY f.id,cl.id", rowCandidate,
            std::filesystem::path(selector.path).filename().string(),
            selector.repo.value_or(""), selector.component.value_or(""),
            selector.clone.value_or(""));
      })
      .transform_error([](const std::string &message) {
        return Error{503, "project_unavailable", message};
      });
}
} // namespace facts::apis::domain::detail
