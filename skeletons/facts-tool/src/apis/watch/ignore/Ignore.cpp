#include "apis/watch/ignore/State.h"
#include <algorithm>

namespace facts::apis::watch {
Ignore::Ignore(std::unique_ptr<Impl> state) : impl_(std::move(state)) {}
Ignore::Ignore(Ignore &&) noexcept = default;
Ignore &Ignore::operator=(Ignore &&) noexcept = default;
Ignore::~Ignore() = default;

std::expected<Ignore, std::string>
Ignore::create(const std::filesystem::path &cloneRoot, const Settings &settings) {
  if (!cloneRoot.is_absolute())
    return std::unexpected("ignore clone root must be absolute");
  auto state = std::make_unique<Impl>();
  state->root = cloneRoot.lexically_normal();
  if (state->root.has_relative_path() && state->root.filename().empty())
    state->root = state->root.parent_path();
  for (const auto &value : settings.excludedDirectories) {
    const std::filesystem::path path(value);
    state->excludedDirectories.push_back(
        (path.is_absolute() ? path : state->root / path).lexically_normal());
  }
  return ignore::openRepository(state->root)
      .and_then([&](ignore::Repository repository)
                    -> std::expected<Ignore, std::string> {
        state->repository = std::move(repository);
        if (git_repository_path(state->repository.get())) {
          git_index *index = nullptr;
          if (git_repository_index(&index, state->repository.get()) < 0)
            return std::unexpected(ignore::error("cannot read clone index"));
          state->index.reset(index);
        }
        return ignore::memoryRepository().and_then(
            [&](ignore::Repository patterns)
                -> std::expected<Ignore, std::string> {
              state->patterns = std::move(patterns);
              std::string rules;
              for (const auto &rule : settings.excludePatterns)
                rules += rule + '\n';
              if (!rules.empty() && git_ignore_add_rule(
                      state->patterns.get(), rules.c_str()) < 0)
                return std::unexpected(ignore::error("cannot add YAML ignores"));
              return Ignore(std::move(state));
            });
      });
}

std::expected<bool, std::string>
Ignore::excludes(const std::filesystem::path &path, bool directory) const {
  const auto candidate = path.lexically_normal();
  const auto relative = candidate.lexically_relative(impl_->root);
  if (relative.empty() || *relative.begin() == "..")
    return std::unexpected("ignore path is outside clone: " + path.string());
  const bool excluded = std::ranges::any_of(
      impl_->excludedDirectories, [&](const auto &root) {
        const auto below = candidate.lexically_relative(root);
        return !below.empty() && *below.begin() != "..";
      });
  if (excluded) return true;
  if (relative == ".") return false;
  auto name = relative.generic_string();
  if (directory) name += '/';
  return ignore::matchesTree(impl_->patterns.get(), relative, directory)
      .and_then([&](bool denied) -> std::expected<bool, std::string> {
        if (denied) return true;
        if (impl_->index &&
            (directory ? git_index_find_prefix(nullptr, impl_->index.get(),
                                                name.c_str()) == 0
                       : git_index_get_bypath(impl_->index.get(), name.c_str(),
                                              0) != nullptr))
          return false;
        return impl_->rules.excludes(impl_->repository.get(), impl_->root,
                                     relative, directory);
      });
}
}
