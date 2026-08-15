#include "shared.hpp"

namespace e2e {

int sharedCounter = 3;
int mergedGlobal = 11;
static int internalGlobal = 13;
constinit int constinitGlobal = 15;
const char *InitializerFixture::name = "static";
Widget constructedWidget = Widget{7};
ConstClass constructedClass = ConstClass(7);
int globalValues[] = {4, 5, 6};

struct X : public MyRecord {};

void fun(MyRecord x, int a) {}

int useOne() {
  int localValue = 99;
  static int localStatic = 100;
  return transform(Widget{2}) + localValue + localStatic;
}

} // namespace e2e
