#include "apis/watch/ignore/Rules.h"
#include <fstream>

namespace facts::apis::watch::ignore {
namespace {
std::expected<Repository, std::string> matcher(const std::string &text) {
  return memoryRepository().and_then([&](Repository repository)
      -> std::expected<Repository, std::string> {
    if (git_ignore_add_rule(repository.get(), text.c_str()) < 0)
      return std::unexpected(error("cannot parse ignore rules"));
    return repository;
  });
}
}

std::expected<std::optional<Rules>, std::string>
readRules(const std::filesystem::path &file) {
  std::error_code statusError;
  const auto status = std::filesystem::symlink_status(file, statusError);
  if (statusError == std::errc::no_such_file_or_directory) return std::nullopt;
  if (statusError)
    return std::unexpected("cannot inspect " + file.string() + ": " +
                           statusError.message());
  if (!std::filesystem::is_regular_file(status)) return std::nullopt;
  std::ifstream input(file);
  if (!input) return std::unexpected("cannot read " + file.string());
  const std::string text{std::istreambuf_iterator<char>(input), {}};
  if (input.bad()) return std::unexpected("cannot read " + file.string());
  // Opposite defaults distinguish an explicit rule from no matching rule.
  // They also preserve literal negations that libgit2 otherwise optimizes
  // away when the rule being reversed lives in a parent .gitignore file.
  return matcher("!*\n" + text).and_then([&](Repository included)
      -> std::expected<std::optional<Rules>, std::string> {
    return matcher("*\n" + text).transform([&](Repository excluded) {
      return std::optional<Rules>(Rules{std::move(included), std::move(excluded)});
    });
  });
}

std::expected<std::optional<bool>, std::string>
matchRules(const Rules &rules, const std::string &relative) {
  return matches(rules.included.get(), relative).and_then([&](bool included)
      -> std::expected<std::optional<bool>, std::string> {
    return matches(rules.excluded.get(), relative).transform([&](bool excluded) {
      return included == excluded ? std::optional<bool>(included) : std::nullopt;
    });
  });
}
}
