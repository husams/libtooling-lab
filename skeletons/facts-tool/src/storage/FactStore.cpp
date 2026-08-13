#include "storage/FactStore.h"

#include <iostream>
#include <ranges>
#include <unordered_set>

namespace facts {

FactStore::FactStore(std::string path) : storage_(std::move(path)) {}

void FactStore::begin() {}

void FactStore::end() {
  const auto files = idsByUsr_ | std::views::values |
                     std::views::transform(&SymbolId::file) |
                     std::ranges::to<std::unordered_set>();
  std::cerr << "facts-tool: " << count() << " symbol(s) recorded from "
            << files.size() << " file(s)\n";
}

void FactStore::remember(std::string_view usr, SymbolId id) {
  if (usr.empty()) {
    return;
  }
  idsByUsr_.insert_or_assign(std::string{usr}, id);
}

bool FactStore::contains(std::string_view usr) const {
  return idsByUsr_.contains(std::string{usr});
}

std::expected<std::optional<SymbolId>, std::error_code>
FactStore::findId(std::string_view usr) {
  if (usr.empty()) {
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  }
  if (const auto cached = idsByUsr_.find(std::string{usr});
      cached != idsByUsr_.end()) {
    return cached->second;
  }
  return storage_.findId(usr).transform([this, usr](auto id) {
    if (id) {
      remember(usr, *id);
    }
    return id;
  });
}

} // namespace facts
