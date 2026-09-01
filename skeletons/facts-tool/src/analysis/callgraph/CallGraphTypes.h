#pragma once

#include "model/Relation.h"
#include "model/RelationSite.h"

#include <vector>

namespace clang {
class CXXMethodDecl;
class CXXRecordDecl;
class FunctionDecl;
}

namespace facts::callgraph {

struct CallFact {
  Relation relation;
  RelationSite site;
  const clang::FunctionDecl *caller = nullptr;
  const clang::FunctionDecl *callee = nullptr;
  const clang::CXXRecordDecl *receiver = nullptr;
  bool virtualCall = false;
};

struct OverrideFact {
  Relation relation;
  RelationSite site;
  const clang::CXXMethodDecl *source = nullptr;
  const clang::CXXMethodDecl *destination = nullptr;
  const clang::CXXRecordDecl *owner = nullptr;
};

struct CallGraphFacts {
  std::vector<CallFact> calls;
  std::vector<OverrideFact> overrides;
  std::vector<CallFact> dispatches;
};

} // namespace facts::callgraph
