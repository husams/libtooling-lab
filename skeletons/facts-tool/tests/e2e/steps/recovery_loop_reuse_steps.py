"""Native extraction and recovery must agree on implicit loop call sites."""
from pytest_bdd import given
from steps.recovery_cpp_reuse_steps import _variant


def _loop(context, source):
    _variant(context, '#pragma message("S021_FRONTEND")\n' + source)


@given("the S-021 library uses a vector range loop")
def vector_range(context):
    _loop(context, '#include <vector>\n'
          'int bridge() { std::vector<int> v{1,2,3}; int s = 0; '
          'for (int x : v) s += x; return s; }\n')


@given("the S-021 library uses a user container range loop")
def user_range(context):
    _loop(context, 'struct R { int values[3]{1,2,3}; '
          'const int* begin() const { return values; } '
          'const int* end() const { return values + 3; } };\n'
          'int bridge() { R r; int s = 0; '
          'for (int x : r) s += x; return s; }\n')


@given("the S-021 library uses an explicit iterator loop")
def iterator_loop(context):
    _loop(context, '#include <vector>\n'
          'int bridge() { std::vector<int> v{1,2,3}; int s = 0; '
          'for (auto it = v.begin(); it != v.end(); ++it) s += *it; '
          'return s; }\n')


@given("the S-021 library passes an initializer list argument")
def initializer_list(context):
    _loop(context, '#include <initializer_list>\n'
          'int sum(std::initializer_list<int> values) { return values.size(); }\n'
          'int bridge() { return sum({1,2,3}); }\n')
