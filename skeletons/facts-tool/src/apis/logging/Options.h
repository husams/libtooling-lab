#pragma once
#include "apis/logging/Level.h"
#include <filesystem>

namespace facts::apis::logging {
struct Options {
  std::filesystem::path file;
  Level level = Level::info;
};
}
