#pragma once

#include "commands/catalog/SymbolData.h"

#include <ftxui/dom/elements.hpp>

namespace facts::ui::symbol {

ftxui::Element renderDetail(const commands::SymbolFact *selected);

} // namespace facts::ui::symbol
