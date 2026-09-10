#pragma once

#if __has_include(<clang/Options/Options.h>)
#include <clang/Options/Options.h>
#else
#include <clang/Driver/Options.h>
#endif
#include <llvm/Option/ArgList.h>
#include <algorithm>
#include <iterator>
#include <string>
#include <unordered_set>
#include <vector>

namespace facts::commands {
#if __has_include(<clang/Options/Options.h>)
namespace compiler_options = clang::options;
namespace compiler_driver = clang;
#else
namespace compiler_options = clang::driver::options;
namespace compiler_driver = clang::driver;
#endif
struct ArgumentSpan {
  std::string key;
  std::size_t begin;
  std::size_t end;
};

inline std::string argumentKey(const llvm::opt::Arg &argument) {
  const auto option = argument.getOption().getUnaliasedOption();
  auto name = option.getPrefixedName().str();
  if ((name == "-D" || name == "-U") && argument.getNumValues()) {
    const std::string macro = argument.getValue();
    return "macro:" + macro.substr(0, macro.find_first_of("=("));
  }
  if (option.matches(compiler_options::OPT_O_Group)) return "-O";
  if (option.getKind() == llvm::opt::Option::InputClass)
    return "input:" + std::string(argument.getValue());
  if ((name == "-Xclang" || name == "-Xpreprocessor" || name == "-mllvm") &&
      argument.getNumValues())
    return name + ":" + std::string(argument.getValue()).substr(
                            0, std::string(argument.getValue()).find('='));
  if (option.getKind() == llvm::opt::Option::UnknownClass)
    name = argument.getSpelling().split('=').first.str();
  for (const auto prefix : {"-fno-", "-mno-", "-Wno-"}) {
    if (name.starts_with(prefix)) name.erase(2, 3);
  }
  if (name == "-Werror=" && argument.getNumValues())
    return name + argument.getValue();
  // Warning switches share a joined option; the warning name is the setting.
  if (name == "-W" && argument.getNumValues()) {
    std::string warning = argument.getValue();
    if (warning.starts_with("no-")) warning.erase(0, 3);
    if (warning.starts_with("error=")) return name + warning;
    return name + warning.substr(0, warning.find('='));
  }
  return name;
}

inline std::vector<ArgumentSpan>
argumentSpans(const std::vector<std::string> &tokens) {
  std::vector<const char *> pointers;
  std::ranges::transform(tokens, std::back_inserter(pointers),
                         [](const auto &token) { return token.c_str(); });
  unsigned missingIndex = 0, missingCount = 0;
  const auto parsed = compiler_driver::getDriverOptTable().ParseArgs(
      pointers, missingIndex, missingCount,
      llvm::opt::Visibility(compiler_options::ClangOption));
  std::vector<ArgumentSpan> spans;
  for (const auto *argument : parsed)
    spans.push_back({argumentKey(*argument), argument->getIndex(), tokens.size()});
  // Preserve incomplete options so Clang still diagnoses malformed defaults.
  if (missingCount)
    spans.push_back({tokens[missingIndex], missingIndex, tokens.size()});
  for (std::size_t index = 1; index < spans.size(); ++index)
    spans[index - 1].end = spans[index].begin;
  return spans;
}

inline std::vector<std::string>
overrideArguments(const std::vector<std::string> &defaults,
                  const std::vector<std::string> &explicitValues) {
  if (explicitValues.empty()) return defaults;
  std::unordered_set<std::string> overridden;
  for (const auto &span : argumentSpans(explicitValues))
    overridden.insert(span.key);
  std::vector<std::string> merged;
  for (const auto &span : argumentSpans(defaults)) {
    if (!overridden.contains(span.key))
      merged.insert(merged.end(), defaults.begin() + span.begin,
                    defaults.begin() + span.end);
  }
  merged.insert(merged.end(), explicitValues.begin(), explicitValues.end());
  return merged;
}
} // namespace facts::commands
