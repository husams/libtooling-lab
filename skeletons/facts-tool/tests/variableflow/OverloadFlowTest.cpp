#include "analysis/variableflow/Engine.h"

#include <clang/Tooling/CompilationDatabase.h>

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

namespace {

class TempSource {
public:
  explicit TempSource(std::string source) {
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
    directory_ = std::filesystem::temp_directory_path() /
                 ("facts-variable-flow-overloads-" + std::to_string(stamp));
    std::filesystem::create_directories(directory_);
    path_ = directory_ / "source.cpp";
    std::ofstream{path_} << source;
  }

  ~TempSource() { std::filesystem::remove_all(directory_); }

  const std::filesystem::path &path() const { return path_; }

private:
  std::filesystem::path directory_;
  std::filesystem::path path_;
};

} // namespace

int main() {
  TempSource source(R"cpp(
#include <cstddef>
namespace overloads {
using Count = int;
template <std::size_t N> struct fixed_string {
  char value[N];
  constexpr fixed_string(const char (&input)[N]) {
    for (std::size_t index = 0; index < N; ++index) value[index] = input[index];
  }
  constexpr bool operator==(const fixed_string &) const = default;
};
template <fixed_string Value> struct strings {};
int bar(bool value) { return value ? 1 : 0; }
int bar(int value) { return value + 1; }
int aliased(Count value) { return value; }
int const_parameter(const int value) { return value; }
int pointee(const int *value) { return *value; }
int callback(int (*value)(int)) { return value(1); }
int literal(strings<"a b"> value) { return sizeof(value); }
int literal(strings<"a  b"> value) { return sizeof(value); }
int zero() { int value = 1; return value; }
int single(int value) { return value; }

struct Methods {
  int call(int value) & { return value; }
  int call(int value) const & { return value + 1; }
  int call(int value) && { return value + 2; }
};

struct Widget {
  int member;
  Widget(int value) : member(value) {}
  Widget(bool value) : member(value ? 1 : 0) {}
};

struct Functor {
  int operator()() { int value = 0; return value; }
  int operator()(int value) { return value; }
  int operator()(bool value) { return value ? 1 : 0; }
};
}
)cpp");
  clang::tooling::FixedCompilationDatabase database(".", {"-std=c++20"});
  const auto run = [&](std::string function) {
    return facts::variableflow::analyse(
        database, {source.path().string()},
        {std::move(function), "value", std::nullopt, std::nullopt});
  };

  const auto boolBar = run("overloads::bar(bool)");
  assert(boolBar.has_value());
  assert(!boolBar->rootFunction.empty());
  const auto byUsr = run(boolBar->rootFunction);
  assert(byUsr.has_value());

  const auto intBar = run("overloads :: bar ( int )");
  assert(intBar.has_value());
  assert(intBar->rootFunction != boolBar->rootFunction);
  const auto ambiguous = run("overloads::bar");
  assert(!ambiguous.has_value());
  assert(ambiguous.error().find("overloads::bar(bool)") != std::string::npos);
  assert(ambiguous.error().find("overloads::bar(int)") != std::string::npos);
  assert(ambiguous.error().find("[USR ") != std::string::npos);
  assert(!run("overloads::bar(char)").has_value());

  assert(run("overloads::aliased(int)").has_value());
  assert(run("overloads::aliased( Count )").has_value());
  assert(run("overloads::const_parameter(int)").has_value());
  assert(run("overloads::const_parameter(const int)").has_value());
  assert(run("overloads::pointee(const int*)").has_value());
  assert(run("overloads::callback(int(*)(int))").has_value());
  assert(run("overloads::callback( int ( * ) ( int ) )").has_value());
  const auto oneSpace = run("overloads::literal(strings<\"a b\">)");
  const auto twoSpaces = run("overloads::literal(strings<\"a  b\">)");
  assert(oneSpace.has_value());
  assert(twoSpaces.has_value());
  assert(oneSpace->rootFunction != twoSpaces->rootFunction);
  assert(run("overloads::zero()").has_value());
  assert(run("::overloads::single(int)").has_value());

  const auto lvalue = run("overloads::Methods::call(int) &");
  const auto constLvalue = run("overloads::Methods::call(int) const &");
  const auto rvalue = run("overloads::Methods::call(int)&&");
  assert(lvalue.has_value());
  assert(constLvalue.has_value());
  assert(rvalue.has_value());
  assert(lvalue->rootFunction != constLvalue->rootFunction);
  assert(lvalue->rootFunction != rvalue->rootFunction);
  assert(constLvalue->rootFunction != rvalue->rootFunction);

  assert(run("overloads::Widget::Widget(int)").has_value());
  assert(run("overloads::Widget::Widget(bool)").has_value());
  assert(run("overloads::Functor::operator()(int)").has_value());
  assert(run("overloads::Functor::operator() ( bool )").has_value());
  assert(run("overloads::Functor::operator()()").has_value());
}
