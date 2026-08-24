#include "references.hpp"

namespace reference_fixture {

int referenceOne() {
  return sharedInline() + templatedOwner<int>() + redeclaredOwner() +
         nestedDeclarations() + Example{}.method();
}

} // namespace reference_fixture
