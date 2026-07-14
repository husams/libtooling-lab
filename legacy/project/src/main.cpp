// main.cpp — drives the Widget. Contains one more NULL for the capstone.
#include "widget.h"
#include <cstddef>

int main() {
    Widget w;
    Widget* root = NULL;        // NULL -> nullptr
    w.set_parent(root);
    w.draw();
    return 0;
}
