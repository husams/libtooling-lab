#pragma once
#include "apis/jobs/Queue.h"
#include <chrono>

namespace facts::apis {
struct Job {
  Json record;
  JobCallback completion;
  bool finished = false;
};
inline std::int64_t jobTimestamp() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch()).count();
}
}
