#include "commands/analyse/CallGraphRecoveryInternal.h"
#include "storage/catalog/File.h"
#include "tooling/CompilationCommandCodec.h"
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/SHA256.h>
#include <string>
#include <string_view>
namespace facts::commands {
namespace {
struct FramedHash {
  llvm::SHA256 hash;
  void field(std::string_view tag, std::string_view value) {
    const auto frame = std::to_string(tag.size()) + ":" + std::string(tag) +
                       std::to_string(value.size()) + ":" + std::string(value);
    hash.update(llvm::StringRef(frame));
  }
  void number(std::string_view tag, auto value) {
    field(tag, std::to_string(value));
  }
  std::string finish() {
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    for (const auto byte : hash.final()) {
      result += digits[byte >> 4U];
      result += digits[byte & 0x0fU];
    }
    return result;
  }
};
void fileFingerprint(FramedHash &hash, FileId id, const catalog::File &file) {
  hash.number("file.id", id);
  hash.number("file.record.id", file.id);
  hash.number("file.directory.id", file.directoryId);
  if (const auto path = catalog::filePath(file))
    hash.field("file.path", path->string());
  else
    hash.field("file.path.error", path.error());
  hash.number("component.id", file.component.id);
  hash.field("component.name", file.component.name);
  hash.field("component.path", file.component.path);
  hash.field("component.kind", file.component.kind);
  hash.field("component.version.present", file.component.version ? "1" : "0");
  if (file.component.version)
    hash.field("component.version", *file.component.version);
  hash.field("component.repository.present", file.component.repositoryId ? "1" : "0");
  if (file.component.repositoryId)
    hash.number("component.repository", *file.component.repositoryId);
  hash.field("file.component", file.componentName);
  hash.field("file.directory", file.directory);
  hash.field("file.name", file.name);
  hash.field("file.options", file.compileOptions);
  hash.field("file.driver", file.driver);
  hash.field("file.working-directory", file.workingDirectory);
  hash.number("file.args-overridden", file.argsOverridden);
  if (file.clone) {
    hash.number("clone.id", file.clone->id);
    hash.number("clone.repository", file.clone->repositoryId);
    hash.field("clone.path", file.clone->path);
    hash.field("clone.label", file.clone->label);
  } else {
    hash.field("clone", "none");
  }
}
void commandFingerprint(FramedHash &hash, FileId id, const StoredCompileFile &command, const StoredCommandAliases &aliases) {
  hash.number("command.id", id);
  hash.field("command.root", command.root.string());
  hash.field("command.path", command.path.string());
  hash.field("command.component", command.componentName);
  hash.field("command.driver", command.driver);
  hash.field("command.working-directory", command.workingDirectory);
  hash.field("command.options", command.options);
  if (const auto decoded = decodeStoredCommand(command, aliases)) {
    hash.field("effective.directory", decoded->Directory);
    for (const auto &argument : decoded->CommandLine)
      hash.field("effective.argument", argument);
  } else {
    hash.field("effective.error", decoded.error());
  }
}
} // namespace
std::string recoveryRegistryFingerprint(const RecoveryContext &context) {
  FramedHash hash;
  hash.field("version", "recovery-registry-v3");
  hash.field("project", context.project);
  for (const auto &[id, file] : context.files)
    fileFingerprint(hash, id, file);
  for (const auto &[id, command] : context.commands)
    commandFingerprint(hash, id, command, context.aliases);
  for (const auto &[name, path] : context.aliases) {
    hash.field("alias.name", name);
    hash.field("alias.path", path);
  }
  for (const auto &[usr, files] : context.index) {
    hash.field("index.usr", usr);
    for (const auto id : files)
      hash.number("index.file", id);
  }
  return hash.finish();
}
} // namespace facts::commands
