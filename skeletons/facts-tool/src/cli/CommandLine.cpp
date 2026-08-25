#include "cli/CommandLine.h"

#include "cli/Options.h"
#include "cli/Verbose.h"
#include "commands/Dependency.h"
#include "commands/Extract.h"
#include "commands/Import.h"

#include <CLI/CLI.hpp>

#include <algorithm>
#include <expected>
#include <format>
#include <iostream>
#include <string>
#include <utility>
#include <variant>

namespace facts::cli {
namespace {

class Parser {
public:
  Parser() : app_("Extract and import C++ project facts", "facts-tool") {
    app_.require_subcommand(1, 1);
    configureExtract(*app_.add_subcommand(
        "extract", "Extract facts using a stored project configuration"));
    configureImport(*app_.add_subcommand(
        "import", "Import compile commands into a project configuration"));
    analysesCommand_ =
        app_.add_subcommand("analyses", "Run explicitly requested analyses");
    analysesCommand_->require_subcommand(1, 1);
    configureDependency(*analysesCommand_->add_subcommand(
        "dependency", "Build the direct include dependency graph"));
  }

  std::expected<Command, int> parse(int argc, char **argv) {
    try {
      app_.parse(argc, argv);
    } catch (const CLI::ParseError &error) {
      return std::unexpected(app_.exit(error));
    }

    if (extractCommand_->parsed()) {
      return makeCommand(std::move(extract_), extractVerbose_);
    }
    return importCommand_->parsed()
               ? makeCommand(std::move(import_), importVerbose_)
               : makeCommand(std::move(dependency_), dependencyVerbose_);
  }

private:
  template <typename Options>
  static Command makeCommand(Options options, bool stagesEnabled) {
    options.verbosity = std::max(options.verbosity, stagesEnabled ? 1 : 0);
    return Command{std::move(options)};
  }

  static void configureVerbosity(CLI::App &command, int &verbosity,
                                 bool &stagesEnabled) {
    command.add_flag("-v", stagesEnabled, "Enable at least stage output");
    command
        .add_option("--verbose", verbosity,
                    "Verbosity level: 0=quiet, 1=stages, 2=details")
        ->check(CLI::Range(0, maximumVerbosity))
        ->type_name("LEVEL");
  }

  void configureExtract(CLI::App &command) {
    extractCommand_ = &command;
    configureVerbosity(command, extract_.verbosity, extractVerbose_);
    command
        .add_option("-o,--output", extract_.output,
                    "SQLite database for extracted facts")
        ->required()
        ->type_name("FILE");
    command
        .add_option("-c,--conf", extract_.configuration,
                    "Full path to the stored project configuration")
        ->required()
        ->type_name("FILE");
    command.add_option(
        "sources", extract_.sources,
        "Source files to extract; defaults to all imported files");
  }

  void configureImport(CLI::App &command) {
    importCommand_ = &command;
    configureVerbosity(command, import_.verbosity, importVerbose_);
    command
        .add_option("-c,--conf", import_.configuration,
                    "Full path for the stored project configuration")
        ->required()
        ->type_name("FILE");
    command
        .add_option("-p,--compilation-database", import_.compilationDatabase,
                    "Directory containing compile_commands.json")
        ->check(CLI::ExistingDirectory)
        ->type_name("DIR");
    command
        .add_option_function<std::string>(
            "--component",
            [this](const std::string &component) {
              import_.components.push_back(component);
            },
            "Project component as name=path; repeatable")
        ->trigger_on_parse()
        ->type_name("NAME=PATH");
    command
        .add_option_function<std::string>(
            "--extra-arg",
            [this](const std::string &argument) {
              import_.extraArguments.push_back(argument);
            },
            "Compiler argument for fixed-command imports; repeatable")
        ->trigger_on_parse()
        ->type_name("ARG");
    command.add_option(
        "sources", import_.sources,
        "Source files to import; filter compilation database commands or use "
        "--extra-arg arguments");
  }

  void configureDependency(CLI::App &command) {
    dependencyCommand_ = &command;
    configureVerbosity(command, dependency_.verbosity, dependencyVerbose_);
    command
        .add_option("-o,--output", dependency_.output,
                    "SQLite database for extracted dependency facts")
        ->required()
        ->type_name("FILE");
    command
        .add_option("-c,--conf", dependency_.configuration,
                    "Full path to the stored project configuration")
        ->required()
        ->type_name("FILE");
    command
        .add_option("sources", dependency_.sources,
                    "Translation-unit roots to analyse")
        ->required();
  }

  CLI::App app_;
  CLI::App *extractCommand_ = nullptr;
  CLI::App *importCommand_ = nullptr;
  CLI::App *analysesCommand_ = nullptr;
  CLI::App *dependencyCommand_ = nullptr;
  ExtractOptions extract_;
  ImportOptions import_;
  DependencyOptions dependency_;
  bool extractVerbose_ = false;
  bool importVerbose_ = false;
  bool dependencyVerbose_ = false;
};

std::string_view commandName(const ExtractOptions &) { return "extract"; }

std::string_view commandName(const ImportOptions &) { return "import"; }

std::string_view commandName(const DependencyOptions &) { return "dependency"; }

std::string commandDetails(const ExtractOptions &options) {
  return std::format("configuration='{}', output='{}', requested_sources={}",
                     options.configuration, options.output,
                     options.sources.size());
}

std::string commandDetails(const ImportOptions &options) {
  return std::format(
      "configuration='{}', compilation_database='{}', requested_sources={}, "
      "components={}",
      options.configuration,
      options.compilationDatabase.empty() ? "fixed commands"
                                          : options.compilationDatabase,
      options.sources.size(), options.components.size());
}

std::string commandDetails(const DependencyOptions &options) {
  return std::format("configuration='{}', output='{}', roots={}",
                     options.configuration, options.output,
                     options.sources.size());
}

std::expected<int, std::string> execute(const ExtractOptions &options) {
  return commands::runExtract(options);
}

std::expected<int, std::string> execute(const ImportOptions &options) {
  return commands::runImport(options);
}

std::expected<int, std::string> execute(const DependencyOptions &options) {
  return commands::runDependency(options);
}

int report(std::expected<int, std::string> result) {
  if (!result) {
    std::cerr << "facts-tool: " << result.error() << '\n';
    return 1;
  }
  return *result;
}

int dispatch(Command command) {
  return std::visit(
      [](auto options) {
        const auto name = commandName(options);
        logVerbose(options.verbosity, 1, "facts-tool: {}: starting", name);
        logVerbose(options.verbosity, 2, "facts-tool: {}: {}", name,
                   commandDetails(options));
        auto result = execute(options);
        logVerbose(options.verbosity, 1, "facts-tool: {}: {}", name,
                   result ? "complete" : "failed");
        return report(std::move(result));
      },
      std::move(command));
}

} // namespace

int run(int argc, char **argv) {
  auto command = Parser{}.parse(argc, argv);
  return command ? dispatch(std::move(*command)) : command.error();
}

} // namespace facts::cli
