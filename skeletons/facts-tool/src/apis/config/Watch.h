#pragma once
#include "apis/config/Settings.h"
#include <yaml-cpp/yaml.h>

namespace facts::apis {
void readWatch(const YAML::Node &watch, Settings &settings);
}
