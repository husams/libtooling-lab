#ifndef FACTS_TOOL_STORAGE_INITIALIZER_H
#define FACTS_TOOL_STORAGE_INITIALIZER_H

#include "model/Initializer.h"
#include "storage/SqliteQuery.h"

#include <optional>
#include <string>

namespace facts::storage {

struct InitializerColumns {
  std::string expression;
  std::string kind;
  std::optional<std::string> value;
};

InitializerColumns initializerColumns(const Initializer &initializer);

std::optional<Initializer> loadInitializer(const Row &row, int expressionColumn,
                                           int kindColumn, int valueColumn);

} // namespace facts::storage

#endif // FACTS_TOOL_STORAGE_INITIALIZER_H
