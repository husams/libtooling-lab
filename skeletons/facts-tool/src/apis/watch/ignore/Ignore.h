#pragma once
#include "apis/config/Settings.h"
#include <expected>
#include <memory>

namespace facts::apis::watch {
// Construct and use on the scanner worker; recreate to refresh Git rules/index.
class Ignore {
public:
  static std::expected<Ignore, std::string>
  create(const std::filesystem::path &cloneRoot, const Settings &settings);
  Ignore(Ignore &&) noexcept;
  Ignore &operator=(Ignore &&) noexcept;
  ~Ignore();
  std::expected<bool, std::string>
  excludes(const std::filesystem::path &path, bool directory) const;
  std::vector<std::filesystem::path> controlFiles() const;
private:
  struct Impl;
  explicit Ignore(std::unique_ptr<Impl> state);
  std::unique_ptr<Impl> impl_;
};
}
