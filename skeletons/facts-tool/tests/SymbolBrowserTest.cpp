#include "ui/symbol/VirtualListModel.h"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

void require(bool condition, std::string_view message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

} // namespace

int main() {
  using namespace facts::ui::symbol;

  ListState state;
  navigate(state, Navigation::end, 100, 5);
  const auto lastPage = visibleRange(state, 100, 5);
  require(state.selected == 99 && state.offset == 95,
          "end navigation did not select the last virtual row");
  require(lastPage.begin == 95 && lastPage.end == 100,
          "viewport rendered more than the five visible rows");

  navigate(state, Navigation::pageUp, 100, 5);
  require(state.selected == 94 && state.offset == 94,
          "page-up did not keep selection inside the viewport");
  navigate(state, Navigation::home, 100, 5);
  require(state.selected == 0 && state.offset == 0,
          "home did not reset selection and scroll offset");

  const auto columns = columnWidths(100);
  require(columns.total() == 100,
          "fixed column widths do not fill the terminal viewport");
  require(columnWidths(38).total() == 38,
          "narrow terminal columns do not match the available viewport");
  const auto truncated = truncateCell("/a/very/long/path/to/source.cpp", 14);
  require(cellWidth(truncated) == 14 && truncated.ends_with("..."),
          "long cells were not truncated to their terminal width");
  const auto unicode = truncateCell("测试-symbol-name", 9);
  require(cellWidth(unicode) <= 9 && unicode.ends_with("..."),
          "UTF-8 truncation exceeded the terminal cell width");

  return 0;
}
