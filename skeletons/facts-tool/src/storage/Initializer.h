#ifndef FACTS_TOOL_STORAGE_INITIALIZER_H
#define FACTS_TOOL_STORAGE_INITIALIZER_H

#include "model/Initializer.h"

#include <optional>

struct sqlite3_stmt;

namespace facts::storage {

bool bindInitializer(sqlite3_stmt *statement, int expressionPosition,
                     int kindPosition, int valuePosition,
                     const Initializer &initializer);

std::optional<Initializer> loadInitializer(sqlite3_stmt *statement,
                                           int expressionColumn, int kindColumn,
                                           int valueColumn);

} // namespace facts::storage

#endif // FACTS_TOOL_STORAGE_INITIALIZER_H
