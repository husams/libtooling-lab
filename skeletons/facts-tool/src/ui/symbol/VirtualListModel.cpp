#include "ui/symbol/VirtualListModel.h"

#include <ftxui/screen/string.hpp>

#include <algorithm>

namespace facts::ui::symbol {
namespace {

constexpr int columnGaps = 1;

std::size_t pageSize(std::size_t viewportHeight) {
  return std::max<std::size_t>(1, viewportHeight);
}

} // namespace

int ColumnWidths::total() const { return kind + path + columnGaps; }

void fitViewport(ListState &state, std::size_t itemCount,
                 std::size_t viewportHeight) {
  if (itemCount == 0) {
    state = {};
    return;
  }
  const auto page = pageSize(viewportHeight);
  state.selected = std::min(state.selected, itemCount - 1);
  if (state.selected < state.offset)
    state.offset = state.selected;
  if (state.selected >= state.offset + page)
    state.offset = state.selected - page + 1;
  const auto maximumOffset = itemCount > page ? itemCount - page : 0;
  state.offset = std::min(state.offset, maximumOffset);
}

void navigate(ListState &state, Navigation movement, std::size_t itemCount,
              std::size_t viewportHeight) {
  if (itemCount == 0)
    return;
  const auto page = pageSize(viewportHeight);
  switch (movement) {
  case Navigation::up:
    state.selected = state.selected == 0 ? 0 : state.selected - 1;
    break;
  case Navigation::down:
    state.selected = std::min(state.selected + 1, itemCount - 1);
    break;
  case Navigation::pageUp:
    state.selected = state.selected > page ? state.selected - page : 0;
    break;
  case Navigation::pageDown:
    state.selected = std::min(state.selected + page, itemCount - 1);
    break;
  case Navigation::home:
    state.selected = 0;
    break;
  case Navigation::end:
    state.selected = itemCount - 1;
    break;
  }
  fitViewport(state, itemCount, viewportHeight);
}

VisibleRange visibleRange(ListState state, std::size_t itemCount,
                          std::size_t viewportHeight) {
  fitViewport(state, itemCount, viewportHeight);
  return {state.offset,
          std::min(itemCount, state.offset + pageSize(viewportHeight))};
}

ColumnWidths columnWidths(int terminalWidth) {
  const auto width = std::max(3, terminalWidth);
  const auto kind = std::min(12, width - 2);
  return {kind, width - kind - columnGaps};
}

int cellWidth(std::string_view value) { return ftxui::string_width(value); }

std::string truncateCell(std::string_view value, int width) {
  if (width <= 0)
    return {};
  if (cellWidth(value) <= width)
    return std::string(value);
  if (width <= 3)
    return std::string(static_cast<std::size_t>(width), '.');

  const auto contentWidth = width - 3;
  std::string result;
  int used = 0;
  for (const auto &glyph : ftxui::Utf8ToGlyphs(value)) {
    const auto glyphWidth = cellWidth(glyph);
    if (used + glyphWidth > contentWidth)
      break;
    result += glyph;
    used += glyphWidth;
  }
  return result + "...";
}

} // namespace facts::ui::symbol
