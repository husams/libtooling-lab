#include "ui/symbol/SymbolDetail.h"

#include "commands/catalog/SymbolFormat.h"

#include <sstream>

namespace facts::ui::symbol {

ftxui::Element renderDetail(const commands::SymbolFact *selected) {
  if (selected == nullptr)
    return ftxui::text("No symbols") | ftxui::center;

  std::istringstream lines(commands::displaySymbol(*selected));
  ftxui::Elements elements;
  std::string line;
  while (std::getline(lines, line))
    elements.push_back(ftxui::paragraph(line));
  return ftxui::vbox(std::move(elements));
}

} // namespace facts::ui::symbol
