#pragma once

#include "analysis/variableflow/FlowSupport.h"

namespace facts::variableflow::detail {
void applyReachingDefinitions(Builder &builder);
}
