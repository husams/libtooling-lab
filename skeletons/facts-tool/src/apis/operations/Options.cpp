#include "apis/operations/Details.h"

namespace facts::apis::operations {
namespace {
template <typename Options>
Options common(const domain::Context &context, const domain::ResolvedFile &file) {
  Options options;
  options.configuration = context.configuration.database.string();
  options.defaultExtraArguments = context.configuration.extraArguments;
  options.sources = {file.path.string()};
  options.astCache = context.configuration.astCache;
  options.astCache.verbosity = 0;
  return options;
}
}
cli::ExtractOptions extractOptions(const domain::Context &context,
                                  const domain::ResolvedFile &file,
                                  const ExtractRequest &request) {
  auto options = common<cli::ExtractOptions>(context, file);
  options.output = file.facts.string();
  options.outputProvided = true;
  options.outputFromTemplate = true;
  options.force = request.force;
  return options;
}
cli::DependencyOptions dependencyOptions(const domain::Context &context,
                                        const domain::ResolvedFile &file) {
  auto options = common<cli::DependencyOptions>(context, file);
  options.output = file.facts.string();
  options.outputProvided = true;
  options.outputFromTemplate = true;
  return options;
}
cli::MatchOptions matchOptions(const domain::Context &context,
                              const domain::ResolvedFile &file,
                              const MatchRequest &request) {
  auto options = common<cli::MatchOptions>(context, file);
  options.facts = file.facts.string();
  options.factsProvided = true;
  options.matcher = request.query;
  options.traversal = request.traversal;
  options.relationKind = request.relationKind;
  options.captureSource = request.captureSource;
  options.format = "json";
  return options;
}
}
