#include "implicit.hpp"
#include "cv.hpp"
#include "specifiers.hpp"
int qualifiers::Cv::split(const int &value) const volatile & noexcept { return value; }
