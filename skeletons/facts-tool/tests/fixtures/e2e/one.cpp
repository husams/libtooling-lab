#include "shared.hpp"

namespace e2e {

int sharedCounter = 3;

struct X : public MyRecord {};

void fun(MyRecord x, int a) {}

int useOne() { return transform(Widget{2}); }

} // namespace e2e
