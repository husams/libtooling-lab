#pragma once

#include <filesystem>

namespace storage_schema_test {

auto verifyReturnTypeStorage(const std::filesystem::path &database) -> bool;
auto verifyFreshSchema(const std::filesystem::path &database) -> bool;
auto verifyMigration(const std::filesystem::path &database) -> bool;
auto verifyVersionOneMigration(const std::filesystem::path &database) -> bool;
auto verifyVersionTwoMigration(const std::filesystem::path &database) -> bool;
auto verifyVersionFiveMigration(const std::filesystem::path &database) -> bool;
auto verifyVersionSevenMigration(const std::filesystem::path &database) -> bool;
auto verifyFileSchemaRollback(const std::filesystem::path &database) -> bool;

} // namespace storage_schema_test
