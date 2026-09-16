#include "tooling/astcache/ExternalInputs.h"
#include "tooling/astcache/FileIdentity.h"

#include <clang/Tooling/CompilationDatabase.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/StringSaver.h>
#include <optional>
#include <set>
#include <span>
#include <string_view>
#include <vector>

namespace facts::astcache::detail {
namespace {
struct Response {
  llvm::json::Object record;
  std::vector<std::string> arguments;
};

std::expected<Response, std::string> readResponse(const fs::path &path) {
  return fileRecord(path).and_then([&](llvm::json::Object record)
      -> std::expected<Response, std::string> {
    auto buffer = llvm::MemoryBuffer::getFile(path.string());
    if (!buffer)
      return std::unexpected(buffer.getError().message());
    if (record.getString("digest") != digest((*buffer)->getBuffer()))
      return std::unexpected("response file changed while reading: " +
                             path.string());
    llvm::BumpPtrAllocator allocator;
    llvm::StringSaver saver(allocator);
    llvm::SmallVector<const char *, 32> tokens;
    llvm::cl::TokenizeGNUCommandLine((*buffer)->getBuffer(), saver, tokens);
    std::vector<std::string> arguments(tokens.begin(), tokens.end());
    return Response{std::move(record), std::move(arguments)};
  });
}

std::optional<std::string> indirectPath(std::span<const std::string> arguments,
                                        std::size_t index) {
  const auto &argument = arguments[index];
  for (const auto *option : {"-include-pch", "-include-pth", "-ivfsoverlay",
                             "-fmodule-map-file", "-fmodule-file"}) {
    std::string value;
    if (argument == option && index + 1 < arguments.size()) {
      const auto operand = index + 1 + (arguments[index + 1] == "-Xclang");
      if (operand >= arguments.size())
        return std::nullopt;
      value = arguments[operand];
    } else if (argument.starts_with(std::string(option) + "=")) {
      value = argument.substr(std::string(option).size() + 1);
    } else {
      continue;
    }
    if (std::string_view(option) == "-fmodule-file" && value.contains('='))
      value = value.substr(value.find('=') + 1);
    return value;
  }
  return std::nullopt;
}
} // namespace

std::expected<llvm::json::Array, std::string>
externalInputs(const clang::tooling::CompileCommand &command,
               const std::filesystem::path &cwd) {
  llvm::json::Array records;
  std::vector<std::vector<std::string>> streams{command.CommandLine};
  std::set<fs::path> responses;
  for (std::size_t stream = 0; stream < streams.size(); ++stream) {
    const auto arguments = streams[stream];
    for (std::size_t index = 0; index < arguments.size(); ++index) {
      const auto &argument = arguments[index];
      if (argument.starts_with('@')) {
        const auto path = resolve(argument.substr(1), cwd);
        if (!responses.insert(path).second)
          continue;
        auto response = readResponse(path);
        if (!response)
          return std::unexpected(response.error());
        records.push_back(std::move(response->record));
        streams.push_back(std::move(response->arguments));
      } else if (const auto path = indirectPath(arguments, index)) {
        auto record = fileRecord(resolve(*path, cwd));
        if (!record)
          return std::unexpected(record.error());
        records.push_back(std::move(*record));
      }
    }
  }
  return records;
}
} // namespace facts::astcache::detail
