#include <cstddef>
#include <string_view>

#if defined(FACTS_EXPECT_LIBSTDCXX) && !defined(__GLIBCXX__)
#error "expected the target GNU libstdc++ headers"
#endif

#if defined(FACTS_EXPECT_LIBCPP) && !defined(_LIBCPP_VERSION)
#error "expected the target libc++ headers"
#endif

namespace qualification {

int standardLibraryProbe() {
  return sizeof(std::string_view) + sizeof(std::size_t);
}

} // namespace qualification
