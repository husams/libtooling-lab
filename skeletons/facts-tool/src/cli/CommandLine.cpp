#include "cli/CommandLine.h"
#include "cli/Dispatch.h"
#include "cli/MatchCommandLine.h"
#include "cli/Verbose.h"
#include "cli/catalog/Configure.h"
#include <CLI/CLI.hpp>
#include <expected>
#include <limits>
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
    matchCommand_ = configureMatch(app_, match_);
    analyseCommand_ =
        app_.add_subcommand("analyse", "Run explicitly requested analyses");
    analyseCommand_->require_subcommand(1, 1);
    configureDependency(*analyseCommand_->add_subcommand(
        "dependency", "Build the direct include dependency graph"));
    configureCallGraph(*analyseCommand_->add_subcommand(
        "call-graph", "Query contextual function call graphs"));
    repositoryCommand_ = configureRepository(app_, repository_);
    componentCommand_ = configureComponent(app_, component_);
    directoryCommand_ = configureDirectory(app_, directory_);
    fileCommand_ = configureFile(app_, file_);
    symbolCommand_ = configureSymbol(app_, symbol_);
    configCommand_ = app_.add_subcommand(
        "config",
        "Inspect YAML defaults with yaml-cpp 0.9.0; lookup is --config, "
        "FACTS_TOOL_CONFIG, project, XDG, HOME; --conf overrides generated "
        "naming and ownership");
    configCommand_->require_subcommand(1, 1);
    auto &show = *configCommand_->add_subcommand(
        "show", "Show resolved YAML defaults and ordered discovery");
    show.add_option_function<std::string>(
            "--config",
            [this](const std::string &value) {
              if (value.empty())
                throw CLI::ValidationError("--config must not be empty");
              config_.configurationFile = value;
            },
            "YAML file; lookup is --config, FACTS_TOOL_CONFIG, project, XDG, HOME")
        ->trigger_on_parse()
        ->type_name("FILE");
    show.add_option_function<std::string>(
            "--conf",
            [this](const std::string &value) {
              if (value.empty())
                throw CLI::ValidationError("--conf must not be empty");
              config_.direct = value;
            },
            "Direct database override; bypasses generated naming and ownership")
        ->trigger_on_parse()
        ->type_name("FILE");
  }

  std::expected<Command, int> parse(int argc, char **argv) {
    try {
      app_.parse(argc, argv);
    } catch (const CLI::ParseError &error) {
      const auto exitCode = app_.exit(error);
      return std::unexpected(exitCode == 0 ? 0 : 2);
    }
    if (extractCommand_->parsed()) {
      return Command{std::move(extract_)};
    }
    if (matchCommand_->parsed())
      return Command{std::move(match_)};
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
    if (configCommand_->parsed()) return Command{std::move(config_)};
    if (callGraphCommand_->parsed())
      return Command{std::move(callGraph_)};
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
                    "Project database; YAML yaml-cpp 0.9.0 resolver supplies defaults")
        ->type_name("FILE");
    command.add_option("--config", extract_.configurationFile,
                       "YAML defaults file (yaml-cpp 0.9.0; --config then env/project/XDG/HOME)");
    command
        .add_option_function<std::string>(
            "--extra-arg",
            [this](const std::string &argument) {
              extract_.extraArguments.push_back(argument);
            },
            "Compiler argument appended after YAML tokens; shell-tokenized and repeatable")
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
                    "Project database; YAML yaml-cpp 0.9.0 resolver supplies defaults")
        ->type_name("FILE");
    command.add_option("--config", import_.configurationFile,
                       "YAML defaults file (yaml-cpp 0.9.0; --config then env/project/XDG/HOME)");
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
            "Compiler argument appended after YAML tokens to fixed-command or "
            "compile_commands.json imports; shell-tokenized and repeatable")
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
                    "Project database; YAML yaml-cpp 0.9.0 resolver supplies defaults")
        ->type_name("FILE");
    command.add_option("--config", dependency_.configurationFile,
                       "YAML defaults file (yaml-cpp 0.9.0; --config then env/project/XDG/HOME)");
    command
        .add_option_function<std::string>(
            "--extra-arg",
            [this](const std::string &argument) {
              dependency_.extraArguments.push_back(argument);
            },
            "Compiler argument appended after YAML tokens; shell-tokenized and repeatable")
        ->trigger_on_parse()
        ->type_name("ARG");
    command
        .add_option("sources", dependency_.sources,
                    "Translation-unit roots to analyse")
        ->required();
  }

  void configureCallGraph(CLI::App &command) {
    callGraphCommand_ = &command;
    configureVerbosity(command, callGraph_.verbosity);
    command.add_option("-f,--facts", callGraph_.facts, "SQLite facts database")
        ->required()
        ->type_name("FILE");
    auto *scope = command.add_option_group("scope", "Select graph roots");
    scope
        ->add_option("--function", callGraph_.function,
                     "Qualified function name or USR")
        ->type_name("SELECTOR");
    scope->add_flag("--all", callGraph_.all,
                    "All definition-backed functions with calls");
    scope->require_option(1, 1);
    command
        .add_option("--max-depth", callGraph_.maxDepth,
                    "Maximum traversal depth")
        ->check(CLI::Range(1, std::numeric_limits<int>::max()))
        ->type_name("N");
  }

  CLI::App app_;
  CLI::App *extractCommand_ = nullptr;
  CLI::App *importCommand_ = nullptr;
  CLI::App *analyseCommand_ = nullptr;
  CLI::App *dependencyCommand_ = nullptr;
  CLI::App *callGraphCommand_ = nullptr;
  CLI::App *matchCommand_ = nullptr;
  ExtractOptions extract_;
  ImportOptions import_;
  DependencyOptions dependency_;
  CallGraphOptions callGraph_;
  MatchOptions match_;
  CLI::App *repositoryCommand_ = nullptr;
  CLI::App *componentCommand_ = nullptr;
  CLI::App *directoryCommand_ = nullptr;
  CLI::App *fileCommand_ = nullptr;
  CLI::App *symbolCommand_ = nullptr;
  CLI::App *configCommand_ = nullptr;
  RepositoryOptions repository_;
  ComponentOptions component_;
  DirectoryOptions directory_;
  FileOptions file_;
  SymbolOptions symbol_;
  ConfigOptions config_;
};
} // namespace

int run(int argc, char **argv) {
  auto command = Parser{}.parse(argc, argv);
  return command ? dispatch(std::move(*command)) : command.error();
}

} // namespace facts::cli
