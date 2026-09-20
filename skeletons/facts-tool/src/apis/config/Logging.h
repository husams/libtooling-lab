#pragma once
#include "apis/config/Settings.h"
#include <yaml-cpp/yaml.h>

namespace facts::apis {
void readLogging(const YAML::Node &node, logging::Options &options);
void writeLogging(YAML::Node &root, const logging::Options &options);
void normalizeLogging(Settings &settings);
}
