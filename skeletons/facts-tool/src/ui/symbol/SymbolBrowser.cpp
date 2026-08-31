#include "ui/symbol/SymbolBrowser.h"

#include "ui/symbol/SymbolDetail.h"
#include "ui/symbol/VirtualList.h"
#include "ui/symbol/VirtualListModel.h"

#include <ftxui/component/app.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/terminal.hpp>

#include <algorithm>
#include <format>
#include <optional>

namespace facts::ui::symbol {
namespace {

std::optional<Navigation> navigationFor(const ftxui::Event &event) {
  if (event == ftxui::Event::ArrowUp || event == ftxui::Event::k)
    return Navigation::up;
  if (event == ftxui::Event::ArrowDown || event == ftxui::Event::j)
    return Navigation::down;
  if (event == ftxui::Event::PageUp)
    return Navigation::pageUp;
  if (event == ftxui::Event::PageDown)
    return Navigation::pageDown;
  if (event == ftxui::Event::Home)
    return Navigation::home;
  if (event == ftxui::Event::End)
    return Navigation::end;
  return std::nullopt;
}

struct LayoutSize {
  int contentHeight;
  int listWidth;
  int rowHeight;
};

LayoutSize layoutSize() {
  const auto terminal = ftxui::Terminal::Size();
  const auto width = terminal.dimx > 0 ? terminal.dimx : 80;
  const auto contentHeight =
      terminal.dimy > 0 ? std::max(8, terminal.dimy - 3) : 21;
  const auto listWidth = std::max(34, width * 3 / 5);
  return {contentHeight, listWidth, contentHeight - 3};
}

} // namespace

int runBrowser(std::vector<commands::SymbolFact> values) {
  auto app = ftxui::App::Fullscreen();
  ListState state;
  auto renderer = ftxui::Renderer([&] {
    const auto size = layoutSize();
    fitViewport(state, values.size(), static_cast<std::size_t>(size.rowHeight));
    const auto *selected =
        values.empty() ? nullptr : &values.at(state.selected);
    auto list =
        ftxui::window(ftxui::text(std::format(" Symbols ({}) ", values.size())),
                      renderVirtualList(values, state, size.listWidth - 2,
                                        size.rowHeight)) |
        ftxui::size(ftxui::WIDTH, ftxui::EQUAL, size.listWidth) |
        ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, size.contentHeight);
    auto detail = ftxui::window(ftxui::text(" Detail "),
                                renderDetail(selected) | ftxui::yframe) |
                  ftxui::flex;
    return ftxui::vbox({
        ftxui::text("facts-tool symbol browser") | ftxui::bold | ftxui::center,
        ftxui::hbox({std::move(list), std::move(detail)}) | ftxui::flex,
        ftxui::text(
            "↑/↓ or j/k select  PgUp/PgDn scroll  Home/End jump  q exit") |
            ftxui::dim,
    });
  });

  auto controller = ftxui::CatchEvent(renderer, [&](const ftxui::Event &event) {
    if (event == ftxui::Event::q || event == ftxui::Event::Escape) {
      app.Exit();
      return true;
    }
    const auto movement = navigationFor(event);
    if (!movement)
      return false;
    navigate(state, *movement, values.size(),
             static_cast<std::size_t>(layoutSize().rowHeight));
    return true;
  });
  app.Loop(controller);
  return 0;
}

} // namespace facts::ui::symbol
