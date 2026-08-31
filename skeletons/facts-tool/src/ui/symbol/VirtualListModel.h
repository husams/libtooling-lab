#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace facts::ui::symbol {

struct ListState {
  std::size_t selected = 0;
  std::size_t offset = 0;
};

struct VisibleRange {
  std::size_t begin;
  std::size_t end;
};

enum class Navigation { up, down, pageUp, pageDown, home, end };

struct ColumnWidths {
  int kind;
  int path;

  [[nodiscard]] int total() const;
};

void fitViewport(ListState &state, std::size_t itemCount,
                 std::size_t viewportHeight);
void navigate(ListState &state, Navigation movement, std::size_t itemCount,
              std::size_t viewportHeight);
[[nodiscard]] VisibleRange visibleRange(ListState state, std::size_t itemCount,
                                        std::size_t viewportHeight);
[[nodiscard]] ColumnWidths columnWidths(int terminalWidth);
[[nodiscard]] int cellWidth(std::string_view value);
[[nodiscard]] std::string truncateCell(std::string_view value, int width);

} // namespace facts::ui::symbol
