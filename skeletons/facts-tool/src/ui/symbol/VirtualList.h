#pragma once

#include "commands/catalog/SymbolData.h"
#include "ui/symbol/VirtualListModel.h"

#include <ftxui/dom/elements.hpp>

#include <vector>

namespace facts::ui::symbol {

ftxui::Element
renderVirtualList(const std::vector<commands::SymbolFact> &values,
                  const ListState &state, int width, int height);

} // namespace facts::ui::symbol
