#include "ui/symbol/VirtualList.h"

#include <algorithm>
#include <array>

namespace facts::ui::symbol {
namespace {

using Row = std::array<std::string, 2>;

ftxui::Element cell(const std::string &value, int width) {
  return ftxui::text(truncateCell(value, width)) |
         ftxui::size(ftxui::WIDTH, ftxui::EQUAL, width);
}

ftxui::Element renderRow(const Row &row, const ColumnWidths &widths,
                         bool selected) {
  auto element = ftxui::hbox(
      {cell(row[0], widths.kind), ftxui::text(" "), cell(row[1], widths.path)});
  return selected ? element | ftxui::inverted : element;
}

Row rowFor(const commands::SymbolFact &value) {
  return {value.kind, value.qualifiedName};
}

} // namespace

ftxui::Element
renderVirtualList(const std::vector<commands::SymbolFact> &values,
                  const ListState &state, int width, int height) {
  const auto widths = columnWidths(width);
  const Row headers{"Kind", "Path"};
  ftxui::Elements rows{renderRow(headers, widths, false) | ftxui::bold};
  const auto range = visibleRange(
      state, values.size(), static_cast<std::size_t>(std::max(1, height)));
  rows.reserve(range.end - range.begin + 2);
  for (auto index = range.begin; index < range.end; ++index)
    rows.push_back(
        renderRow(rowFor(values[index]), widths, index == state.selected));
  rows.push_back(ftxui::filler());
  return ftxui::vbox(std::move(rows));
}

} // namespace facts::ui::symbol
