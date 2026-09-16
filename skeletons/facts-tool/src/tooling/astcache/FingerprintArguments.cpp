#include "tooling/astcache/FingerprintArguments.h"

#include "tooling/astcache/FileIdentity.h"

#if __has_include(<clang/Options/Options.h>)
#include <clang/Options/Options.h>
#else
#include <clang/Driver/Options.h>
#endif
#include <llvm/Option/ArgList.h>

#include <algorithm>
#include <string_view>

namespace facts::astcache::detail {
namespace {
#if __has_include(<clang/Options/Options.h>)
namespace compiler_options = clang::options;
namespace compiler_driver = clang;
#else
namespace compiler_options = clang::driver::options;
namespace compiler_driver = clang::driver;
#endif

bool directoryOption(const llvm::opt::Arg &argument) {
  const auto name =
      argument.getOption().getUnaliasedOption().getPrefixedName().str();
  constexpr std::string_view options[] = {
      "-I", "-isystem", "-iquote", "-idirafter", "-F", "-iframework",
      "-isysroot", "--sysroot=", "--sysroot", "-resource-dir"};
  return std::ranges::find(options, name) != std::ranges::end(options);
}

bool sourceOperand(const std::string &value, const fs::path &source,
                   const fs::path &cwd) {
  const auto path = resolve(value, cwd);
  if (path == source)
    return true;
  std::error_code error;
  return fs::equivalent(path, source, error) && !error;
}

std::string directoryOperand(std::string_view value, const fs::path &cwd) {
  // These spellings are relative to the compiler's sysroot, not its cwd.
  if (value.empty() || value.starts_with('=') || value.starts_with("$SYSROOT"))
    return std::string(value);
  const auto path = resolve(std::string(value), cwd);
  // Resolve symlinks before '..'; lexical collapse can name a different path.
  std::error_code error;
  const auto canonical = fs::weakly_canonical(path, error);
  return error ? path.string() : canonical.string();
}

void normalize(const llvm::opt::Arg &argument,
               std::vector<std::string> &result, const fs::path &source,
               const fs::path &cwd) {
  const auto index = argument.getIndex() + 1;
  if (argument.getOption().getKind() == llvm::opt::Option::InputClass) {
    if (sourceOperand(argument.getValue(), source, cwd))
      result[index] = source.string();
    return;
  }
  if (!directoryOption(argument) || argument.getNumValues() != 1)
    return;
  const auto value = directoryOperand(argument.getValue(), cwd);
  const auto spelling = argument.getSpelling().str();
  if (result[index] == spelling)
    result[index + 1] = value;
  else
    result[index] = spelling + value;
}
} // namespace

std::vector<std::string>
fingerprintArguments(const std::vector<std::string> &arguments,
                     const fs::path &source, const fs::path &workingDirectory) {
  if (arguments.empty())
    return arguments;
  std::vector<const char *> pointers;
  for (std::size_t index = 1; index < arguments.size(); ++index)
    pointers.push_back(arguments[index].c_str());
  unsigned missingIndex = 0, missingCount = 0;
  const auto parsed = compiler_driver::getDriverOptTable().ParseArgs(
      pointers, missingIndex, missingCount,
      llvm::opt::Visibility(compiler_options::ClangOption));
  auto result = arguments;
  for (const auto *argument : parsed)
    normalize(*argument, result, source, workingDirectory);
  return result;
}
} // namespace facts::astcache::detail
