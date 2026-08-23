#pragma once

#include <memory>
#include <string>
#include <vector>

namespace clang::tooling {
class FrontendActionFactory;
}

namespace facts {

struct IncludePath {
  std::string source;
  std::string destination;
};

struct IncludeGraphFacts {
  std::vector<std::string> visitedSources;
  std::vector<IncludePath> edges;
};

std::unique_ptr<clang::tooling::FrontendActionFactory>
createIncludeVisitorFactory(IncludeGraphFacts &facts);

} // namespace facts
