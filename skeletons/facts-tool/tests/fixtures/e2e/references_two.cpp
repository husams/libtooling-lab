#include "references.hpp"

namespace reference_fixture {

int referenceTwo() {
  return sharedInline() + templatedOwner<int>() + templatedOwner<long>() +
         nestedDeclarations();
}

} // namespace reference_fixture
