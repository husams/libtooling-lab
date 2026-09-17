#include <memory>
#include <vector>

struct Widget {
    ~Widget() {}
};

void caller() {
    std::vector<std::unique_ptr<Widget>> items;
    items.push_back(std::make_unique<Widget>());
}
