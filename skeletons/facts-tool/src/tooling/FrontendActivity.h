#pragma once

#include <llvm/ADT/StringRef.h>
#include <llvm/Support/raw_ostream.h>

namespace facts {

// Report actual front-end work, rather than inferring it from cache misses.
// A miss may fall back without parsing, while a warm consumer should emit none
// of these events. Verbose CLI runs and performance tests share this evidence.
inline void reportFrontendActivity(int verbosity, llvm::StringRef activity,
                                   llvm::StringRef source) {
  if (verbosity >= 1)
    llvm::errs() << "frontend: " << activity << " " << source << '\n';
}

} // namespace facts
