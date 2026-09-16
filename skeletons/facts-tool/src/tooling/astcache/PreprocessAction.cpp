#include "tooling/astcache/PreprocessAction.h"

#include "ast/visitors/IncludeVisitor.h"
#include "tooling/astcache/Metadata.h"
#include "tooling/astcache/RevisionObserver.h"
#include "tooling/astcache/Snapshot.h"

#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Tooling/Tooling.h>

namespace facts::astcache::detail {
namespace {

class SnapshotAction final : public clang::PreprocessOnlyAction {
public:
  SnapshotAction(const Entry &entry, IncludeGraphFacts &includes,
                 CapturedSnapshot &snapshot, RevisionObservations &observations)
      : entry_(entry), includes_(includes), snapshot_(snapshot),
        observations_(observations) {}

protected:
  bool BeginSourceFileAction(clang::CompilerInstance &compiler) override {
    attachRevisionObserver(compiler, entry_.working_directory, observations_);
    attachIncludedFiles(compiler, includes_);
    return true;
  }

  void EndSourceFileAction() override {
    auto &compiler = getCompilerInstance();
    if (!compiler.getDiagnostics().hasErrorOccurred())
      snapshot_ = validateObservedRevisions(observations_).and_then([&] {
        return captureSnapshot(entry_, compiler.getSourceManager(),
                               compiler.getPreprocessor(), includes_);
      });
  }

private:
  const Entry &entry_;
  IncludeGraphFacts &includes_;
  CapturedSnapshot &snapshot_;
  RevisionObservations &observations_;
};

class SnapshotActionFactory final
    : public clang::tooling::FrontendActionFactory {
public:
  SnapshotActionFactory(const Entry &entry, IncludeGraphFacts &includes,
                        CapturedSnapshot &snapshot,
                        RevisionObservations &observations)
      : entry_(entry), includes_(includes), snapshot_(snapshot),
        observations_(observations) {}

  std::unique_ptr<clang::FrontendAction> create() override {
    return std::make_unique<SnapshotAction>(entry_, includes_, snapshot_,
                                           observations_);
  }

private:
  const Entry &entry_;
  IncludeGraphFacts &includes_;
  CapturedSnapshot &snapshot_;
  RevisionObservations &observations_;
};

} // namespace

std::unique_ptr<clang::tooling::FrontendActionFactory>
createPreprocessFactory(const Entry &entry, IncludeGraphFacts &includes,
                        CapturedSnapshot &snapshot,
                        RevisionObservations &observations) {
  return std::make_unique<SnapshotActionFactory>(entry, includes, snapshot,
                                                observations);
}

} // namespace facts::astcache::detail
