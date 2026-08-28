#pragma once

#include <filesystem>

namespace storage_schema_test {

auto verifyFreshSchema(const std::filesystem::path &database) -> bool;
auto verifyMigration(const std::filesystem::path &database) -> bool;
auto verifyVersionOneMigration(const std::filesystem::path &database) -> bool;
auto verifyVersionTwoMigration(const std::filesystem::path &database) -> bool;
auto verifyVersionFiveMigration(const std::filesystem::path &database) -> bool;

} // namespace storage_schema_test
