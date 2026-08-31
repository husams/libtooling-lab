#include "cli/CommandLine.h"

#include "cli/Dispatch.h"
#include "cli/Verbose.h"
#include "cli/catalog/Configure.h"

#include <CLI/CLI.hpp>

#include <expected>
#include <utility>

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
    analyseCommand_ =
        app_.add_subcommand("analyse", "Run explicitly requested analyses");
    analyseCommand_->require_subcommand(1, 1);
    configureDependency(*analyseCommand_->add_subcommand(
        "dependency", "Build the direct include dependency graph"));
    repositoryCommand_ = configureRepository(app_, repository_);
    componentCommand_ = configureComponent(app_, component_);
    directoryCommand_ = configureDirectory(app_, directory_);
    fileCommand_ = configureFile(app_, file_);
    symbolCommand_ = configureSymbol(app_, symbol_);
  }

  std::expected<Command, int> parse(int argc, char **argv) {
    try {
      app_.parse(argc, argv);
    } catch (const CLI::ParseError &error) {
      return std::unexpected(app_.exit(error));
    }

    if (extractCommand_->parsed()) {
      return Command{std::move(extract_)};
    }
    if (repositoryCommand_->parsed())
      return Command{std::move(repository_)};
    if (componentCommand_->parsed())
      return Command{std::move(component_)};
    if (directoryCommand_->parsed())
      return Command{std::move(directory_)};
    if (fileCommand_->parsed())
      return Command{std::move(file_)};
    if (symbolCommand_->parsed())
      return Command{std::move(symbol_)};
    return importCommand_->parsed() ? Command{std::move(import_)}
                                    : Command{std::move(dependency_)};
  }

private:
  static void configureVerbosity(CLI::App &command, int &verbosity) {
    command
        .add_option("-v,--verbose", verbosity,
                    "Verbosity level: 0=quiet, 1=stages, 2=details, 3=trace; "
                    "defaults to 1 when omitted")
        ->expected(0, 1)
        ->default_str("1")
        ->check(CLI::Range(0, maximumVerbosity))
        ->type_name("LEVEL");
  }

  void configureExtract(CLI::App &command) {
    extractCommand_ = &command;
    configureVerbosity(command, extract_.verbosity);
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
    command
        .add_option_function<std::string>(
            "--extra-arg",
            [this](const std::string &argument) {
              extract_.extraArguments.push_back(argument);
            },
            "Compiler argument appended to stored commands; repeatable")
        ->trigger_on_parse()
        ->type_name("ARG");
    command.add_option(
        "sources", extract_.sources,
        "Source files to extract; defaults to all imported files");
  }

  void configureImport(CLI::App &command) {
    importCommand_ = &command;
    configureVerbosity(command, import_.verbosity);
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
            "Compiler argument appended to fixed-command or "
            "compile_commands.json imports; repeatable")
        ->trigger_on_parse()
        ->type_name("ARG");
    command.add_option(
        "sources", import_.sources,
        "Source files to import; filter compilation database commands or use "
        "--extra-arg arguments");
  }

  void configureDependency(CLI::App &command) {
    dependencyCommand_ = &command;
    configureVerbosity(command, dependency_.verbosity);
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
        .add_option_function<std::string>(
            "--extra-arg",
            [this](const std::string &argument) {
              dependency_.extraArguments.push_back(argument);
            },
            "Compiler argument appended to stored commands; repeatable")
        ->trigger_on_parse()
        ->type_name("ARG");
    command
        .add_option("sources", dependency_.sources,
                    "Translation-unit roots to analyse")
        ->required();
  }

  CLI::App app_;
  CLI::App *extractCommand_ = nullptr;
  CLI::App *importCommand_ = nullptr;
  CLI::App *analyseCommand_ = nullptr;
  CLI::App *dependencyCommand_ = nullptr;
  ExtractOptions extract_;
  ImportOptions import_;
  DependencyOptions dependency_;
  CLI::App *repositoryCommand_ = nullptr;
  CLI::App *componentCommand_ = nullptr;
  CLI::App *directoryCommand_ = nullptr;
  CLI::App *fileCommand_ = nullptr;
  CLI::App *symbolCommand_ = nullptr;
  RepositoryOptions repository_;
  ComponentOptions component_;
  DirectoryOptions directory_;
  FileOptions file_;
  SymbolOptions symbol_;
};

} // namespace

int run(int argc, char **argv) {
  auto command = Parser{}.parse(argc, argv);
  return command ? dispatch(std::move(*command)) : command.error();
}

} // namespace facts::cli
