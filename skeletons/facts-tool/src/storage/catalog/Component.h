#pragma once
#include "storage/catalog/Records.h"
#include "storage/catalog/Requests.h"

namespace facts::catalog {
Result<std::vector<Component>> components(Database &database);
Result<Component> component(Database &database, const Selector &selector);
Result<void> addComponent(Database &database,
                          const ComponentRegistration &options);
Result<void> detachComponents(Database &database, std::int64_t repositoryId);
Result<void> setVersion(Database &database, const Component &component,
                        const std::string &version);
Result<void> removeComponent(Database &database, std::int64_t id);
} // namespace facts::catalog
