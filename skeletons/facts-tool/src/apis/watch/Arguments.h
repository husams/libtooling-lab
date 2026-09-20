#pragma once
#include <algorithm>
#include <string>
#include <vector>

namespace facts::apis::watch {
inline void enableFlag(std::vector<std::string> &values, const std::string &flag) {
  const auto separator = std::find(values.begin(), values.end(), "--");
  values.erase(std::remove_if(values.begin(), separator, [&](const auto &value) {
    return value == flag || value.starts_with(flag + "=");
  }), separator);
  values.insert(std::find(values.begin(), values.end(), "--"), flag);
}
}
