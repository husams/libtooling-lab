#ifndef FACTS_TOOL_MODEL_FILERECORD_H
#define FACTS_TOOL_MODEL_FILERECORD_H

#include "model/SymbolId.h"

#include <string>

namespace facts {

struct FileRecord {
  FileId id;
  std::string path;
};

} // namespace facts

#endif
