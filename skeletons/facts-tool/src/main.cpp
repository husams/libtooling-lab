#include "cli/CommandLine.h"
#include "apis/Server.h"
#include <string_view>

int main(int argc, char **argv) {
  if (argc > 1 && std::string_view(argv[1]) == "serve")
    return facts::apis::run(argc, argv, facts::cli::commandPaths());
  return facts::cli::run(argc, argv);
}
