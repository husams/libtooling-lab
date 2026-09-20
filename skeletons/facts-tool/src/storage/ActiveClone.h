#pragma once
#include <cstdint>
#include <expected>
#include <system_error>

namespace facts::storage {
class Database;
// Changes the persisted default and invalidates only that repository's
// extraction freshness, atomically. Re-selecting the default is a no-op.
std::expected<void, std::error_code>
activateRegisteredClone(Database &database, std::int64_t repository,
                         std::int64_t clone);
}
