#include "commands/analyse/CallGraphOutput.h"
#include <iostream>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>

namespace facts::commands {
std::expected<void, std::string> writeGraphOutput(const std::string &path,
                                                  std::string_view text) {
  if (path.empty()) {
    std::cout << text;
    if (!std::cout)
      return std::unexpected("cannot write graph to stdout");
    return {};
  }
  int descriptor = -1;
  llvm::SmallString<256> temporary;
  auto error = llvm::sys::fs::createUniqueFile(path + ".tmp-%%%%%%", descriptor,
                                               temporary);
  if (error)
    return std::unexpected("cannot create graph output: " + error.message());
  llvm::raw_fd_ostream stream(descriptor, true);
  stream << text;
  stream.close();
  error = stream.error();
  stream.clear_error();
  if (!error)
    error = llvm::sys::fs::rename(temporary, path);
  if (error) {
    (void)llvm::sys::fs::remove(temporary);
    return std::unexpected("cannot publish graph output: " + error.message());
  }
  return {};
}
} // namespace facts::commands
