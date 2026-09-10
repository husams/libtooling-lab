#include "commands/ArgumentOverrides.h"
#include <cassert>

namespace configuration_test {
void argumentOverrides() {
  using facts::commands::overrideArguments;
  const std::vector<std::string> defaults{
      "-std=c++17", "-DVALUE=1", "-DKEEP=1", "-I", "yaml path",
      "-include", "yaml.hpp", "-isystem", "system path", "-fno-exceptions"};
  const auto original = defaults;
  assert(overrideArguments(defaults, {}) == defaults);
  const std::vector<std::string> cli{
      "--std=c++23", "-D", "VALUE=2", "-Icli path", "-fexceptions"};
  auto expected = std::vector<std::string>{
      "-DKEEP=1", "-include", "yaml.hpp", "-isystem", "system path"};
  expected.insert(expected.end(), cli.begin(), cli.end());
  assert(overrideArguments(defaults, cli) == expected);
  assert(defaults == original);
  assert(overrideArguments({"-DVALUE=1", "-DKEEP=1"}, {"-UVALUE"}) ==
         std::vector<std::string>({"-DKEEP=1", "-UVALUE"}));
  assert(overrideArguments({"-U", "VALUE", "-DFUNC(x)=x"},
                           {"-DVALUE=2", "-DFUNC(x)=2*x"}) ==
         std::vector<std::string>({"-DVALUE=2", "-DFUNC(x)=2*x"}));
  assert(overrideArguments({"-Iold", "-I", "other", "-include", "old.hpp"},
                           {"-I", "first", "-Isecond", "-include", "new.hpp"}) ==
         std::vector<std::string>({"-I", "first", "-Isecond", "-include", "new.hpp"}));
  assert(overrideArguments({"-O0", "-Wall", "-Wno-unused", "-mno-avx"},
                           {"-O2", "-Wunused", "-mavx"}) ==
         std::vector<std::string>({"-Wall", "-O2", "-Wunused", "-mavx"}));
  assert(overrideArguments({"--sysroot=/old", "--target=x86_64-linux-gnu"},
                           {"--sysroot", "/new", "-target", "aarch64-linux-gnu"}) ==
         std::vector<std::string>({"--sysroot", "/new", "-target", "aarch64-linux-gnu"}));
  assert(overrideArguments({"-DKEEP=1", "-DVALUE=1"},
                           {"-DVALUE=2", "-DVALUE=3"}) ==
         std::vector<std::string>({"-DKEEP=1", "-DVALUE=2", "-DVALUE=3"}));
  assert(overrideArguments({"-ObjC", "-O0", "-Werror=unused", "-Werror=deprecated"},
                           {"-O2", "-Wno-error=unused"}) ==
         std::vector<std::string>({"-ObjC", "-Werror=deprecated", "-O2", "-Wno-error=unused"}));
  assert(overrideArguments({"-Xclang", "-foo=old", "-Xclang", "-bar"},
                           {"-Xclang", "-foo=new"}) ==
         std::vector<std::string>({"-Xclang", "-bar", "-Xclang", "-foo=new"}));
}
} // namespace configuration_test
