#pragma once

#include "commands/analyse/RecoveryEvidenceScanner.h"
#include "clang/Basic/SourceLocation.h"

#include <string_view>

namespace clang {
class ASTContext;
class FunctionDecl;
class SourceManager;
} // namespace clang

namespace facts::commands {
void appendRecoveryCall(RecoveryBodyFacts &, std::string_view owner,
                        const clang::FunctionDecl &, clang::SourceLocation,
                        bool implicit, const clang::SourceManager &);
bool collectRecoveryBodyEvidence(const clang::FunctionDecl &,
                                 clang::ASTContext &,
                                 const clang::SourceManager &,
                                 std::string_view owner, RecoveryBodyFacts &);
void collectRecoveryDestructorEvidence(const clang::FunctionDecl &,
                                       clang::ASTContext &,
                                       const clang::SourceManager &,
                                       std::string_view owner,
                                       RecoveryBodyFacts &);
} // namespace facts::commands
