#pragma once
#include "apis/operations/Compilation.h"
#include "model/SymbolId.h"
#include <set>

namespace facts::apis::operations::compilation {
struct Candidate {
  FileId id;
  clang::tooling::CompileCommand command;
};
struct Group {
  clang::tooling::CompileCommand transferred;
  std::vector<Candidate> sources;
};
domain::Result<bool> hasCommand(const domain::Context &, const domain::ResolvedFile &);
domain::Result<std::vector<Candidate>> candidates(const domain::Context &);
domain::Result<std::set<FileId>> knownIncluders(const domain::ResolvedFile &);
std::vector<Group> groupCandidates(std::vector<Candidate>,
    const std::filesystem::path &header, const std::set<FileId> &known);
domain::Result<clang::tooling::CompileCommand> selectContext(
    const domain::Context &, const domain::ResolvedFile &, std::vector<Group>);
}
