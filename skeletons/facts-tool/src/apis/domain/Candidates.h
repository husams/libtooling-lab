#pragma once
#include "apis/domain/Selection.h"
#include "storage/catalog/File.h"

namespace facts::apis::domain::detail {
struct Candidate {
  catalog::File file;
  std::string repository;
  bool active = true;
};
Result<void> validateSelector(const FileSelector &selector);
Result<std::vector<Candidate>> candidates(const Context &context,
                                         const FileSelector &selector);
Result<bool> matches(const Candidate &candidate, const FileSelector &selector);
Result<std::filesystem::path> factsPath(const Context &context,
                                       const Candidate &candidate,
                                       const std::filesystem::path &source);
} // namespace facts::apis::domain::detail
