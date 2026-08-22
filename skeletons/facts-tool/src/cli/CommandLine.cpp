#include "cli/CommandLine.h"

#include "cli/Options.h"
#include "commands/Extract.h"
#include "commands/Import.h"

#include <CLI/CLI.hpp>

#include <concepts>
#include <expected>
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
  }

  std::expected<Command, int> parse(int argc, char **argv) {
    try {
      app_.parse(argc, argv);
    } catch (const CLI::ParseError &error) {
      return std::unexpected(app_.exit(error));
    }

    return extractCommand_->parsed() ? Command{std::move(extract_)}
                                     : Command{std::move(import_)};
  }

private:
  void configureExtract(CLI::App &command) {
    extractCommand_ = &command;
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

  CLI::App app_;
  CLI::App *extractCommand_ = nullptr;
  CLI::App *importCommand_ = nullptr;
  ExtractOptions extract_;
  ImportOptions import_;
};

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
        using Options = decltype(options);
        if constexpr (std::same_as<Options, ExtractOptions>) {
          return report(commands::runExtract(options));
        } else {
          return report(commands::runImport(options));
        }
      },
      std::move(command));
}

} // namespace

int run(int argc, char **argv) {
  auto command = Parser{}.parse(argc, argv);
  return command ? dispatch(std::move(*command)) : command.error();
}

} // namespace facts::cli
