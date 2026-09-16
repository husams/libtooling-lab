#include "config/ConfigurationAstCache.h"
#include "config/ConfigurationDiscovery.h"

namespace facts::config::detail {
namespace {

template <class T, class Apply>
void mergeSetting(const MergeContext &context, std::optional<T> Tier::*member,
                  Apply apply) {
  for (const auto *tier : {&context.configFile, &context.project, &context.user}) {
    if (!*tier) continue;
    const auto &setting = (**tier).*member;
    if (!setting) continue;
    apply(*setting, (**tier).path.string());
    return;
  }
}

std::expected<std::filesystem::path, std::string>
directory(const Resolved &value) {
  auto path = value.astCache.directory;
  if (path.empty()) path = ".facts-tool/ast-cache";
  if (path.string().starts_with("~/")) {
    if (env("HOME").empty()) return std::unexpected("requires HOME for ~/ expansion");
    path = std::filesystem::path(env("HOME")) / path.string().substr(2);
  }
  if (path.is_relative()) path = value.projectRoot / path;
  std::error_code error;
  const auto result = std::filesystem::weakly_canonical(path, error);
  if (error) return std::unexpected("cannot resolve: " + error.message());
  return result;
}

} // namespace

void mergeAstCache(Resolved &value, const MergeContext &context) {
  mergeSetting(context, &Tier::astCache, [&](bool enabled, const auto &source) {
    value.astCache.enabled = enabled;
    value.astCacheSource = source;
  });
  mergeSetting(context, &Tier::astCacheDirectory, [&](const auto &path, const auto &source) {
    value.astCache.directory = path;
    value.astCacheDirectorySource = source;
  });
}

std::expected<Resolved, std::string> resolveAstCache(Resolved value) {
  return directory(value)
      .transform_error([&](const auto &reason) {
        return settingError("ast_cache_dir", value.astCacheDirectorySource,
                            reason, value.discovery);
      })
      .transform([&](auto path) {
        value.astCache.directory = std::move(path);
        return value;
      });
}

} // namespace facts::config::detail
