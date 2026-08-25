#ifndef FACTS_TOOL_STORAGE_FILE_ACCESS_H
#define FACTS_TOOL_STORAGE_FILE_ACCESS_H

namespace facts {

// How a command intends to use the project configuration and file registry.
// Import populates it; every later command consumes it without mutating it.
enum class FileAccess { readWrite, readOnly };

} // namespace facts

#endif // FACTS_TOOL_STORAGE_FILE_ACCESS_H
