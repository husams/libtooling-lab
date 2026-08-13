// model/Location.h — a position in a file, copied out of the AST.
//
// Plain data only. Line and column are 1-based, as Clang reports them; offset
// is the byte offset into the file's buffer.

#ifndef FACTS_TOOL_MODEL_LOCATION_H
#define FACTS_TOOL_MODEL_LOCATION_H

namespace facts {

struct Location {
  unsigned line = 0;
  unsigned column = 0;
  unsigned offset = 0;
};

// The slice of the file a symbol covers — a class body, a function definition.
// It exists to be cut out of the buffer, and a substring needs no line or
// column: buffer.substr(offset, size) is the text.
struct Region {
  unsigned offset = 0; // byte offset of the first character
  unsigned size = 0;   // byte length, so offset + size is one past the last
};

} // namespace facts

#endif // FACTS_TOOL_MODEL_LOCATION_H
