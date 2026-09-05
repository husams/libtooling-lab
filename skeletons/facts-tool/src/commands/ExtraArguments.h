#pragma once

#include <cctype>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace facts::commands {

inline std::expected<std::vector<std::string>, std::string>
tokenizeExtraArgument(std::string_view argument) {
  std::vector<std::string> tokens;
  std::string token;
  char quote = '\0';
  bool escaped = false;
  bool started = false;
  for (const char character : argument) {
    if (escaped) {
      token.push_back(character);
      escaped = false;
      started = true;
      continue;
    }
    if (quote != '\'' && character == '\\') {
      escaped = true;
      started = true;
      continue;
    }
    if (quote == '\0' && (character == '\'' || character == '"')) {
      quote = character;
      started = true;
      continue;
    }
    if (quote != '\0' && character == quote) {
      quote = '\0';
      continue;
    }
    if (quote == '\0' && std::isspace(static_cast<unsigned char>(character))) {
      if (started) {
        tokens.push_back(std::move(token));
        token.clear();
        started = false;
      }
      continue;
    }
    token.push_back(character);
    started = true;
  }
  if (escaped || quote != '\0') {
    return std::unexpected("unterminated quoting in --extra-arg: " +
                           std::string(argument));
  }
  if (started) {
    tokens.push_back(std::move(token));
  }
  return tokens;
}

inline std::expected<std::vector<std::string>, std::string>
tokenizeExtraArguments(const std::vector<std::string> &arguments) {
  std::vector<std::string> tokens;
  for (const auto &argument : arguments) {
    auto tokenized = tokenizeExtraArgument(argument);
    if (!tokenized) {
      return std::unexpected(tokenized.error());
    }
    tokens.insert(tokens.end(), tokenized->begin(), tokenized->end());
  }
  return tokens;
}

} // namespace facts::commands
