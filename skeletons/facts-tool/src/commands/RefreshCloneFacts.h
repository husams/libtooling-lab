#pragma once
#include "commands/FactPairValidation.h"

namespace facts {
class FileManager;
namespace commands {
std::expected<void, std::string> refreshCloneFacts(
    FactStore &store, const FactPairProvenanceSnapshot &snapshot,
    FileManager &registry, const std::vector<std::string> &files);
}
}
