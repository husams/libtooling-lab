// widget.cpp — defines build_label (the too-many-parameters offender).
#include "widget.h"

std::string build_label(const std::string& a, const std::string& b,
                        int x, int y, int z, bool flag) {
    (void)x; (void)y; (void)z;
    return flag ? a : b;
}
