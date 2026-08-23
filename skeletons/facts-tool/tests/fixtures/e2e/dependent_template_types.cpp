namespace b004 {

struct Widget {};

namespace support {
struct Marker {};
} // namespace support

using namespace support;

inline constexpr auto migrationSql = "ALTER TABLE symbol";
using Writer = bool (*)(const Widget &, unsigned long);

template <typename T>
T identity(T value) {
  return value;
}

template <>
Widget identity(Widget value) {
  return value;
}

template <typename T>
T *pointer(T *value) {
  return value;
}

template <typename T>
const T &lvalue(const T &value) {
  return value;
}

template <typename T>
T &&rvalue(T &&value) {
  return static_cast<T &&>(value);
}

template <typename Value>
auto dependentAlias(Value &&value) {
  using Owned = decltype(identity(*value));
  return Owned{};
}

template <typename Value>
auto ownBind(Value value) {
  return value;
}

template <typename Value>
auto deducedAlias(Value *value) {
  using Owned = decltype(ownBind(*value));
  return Owned{};
}

template <typename Map, typename... Values>
auto valueStream(Map map, Values... values) -> decltype(map(values...)) {
  return map(values...);
}

bool writeWidget(const Widget &, unsigned long) { return true; }

template <typename... Values>
void parameterPack(Values &&...values) {
  ((void)values, ...);
}

template <typename T>
struct Nested {
  template <typename U>
  static U relay(U value) {
    return value;
  }

  static T instantiate(T value) { return relay<T>(value); }
};

Widget instantiate(Widget value) {
  auto *address = &value;
  (void)identity(1);
  (void)pointer(address);
  (void)lvalue(value);
  (void)rvalue(static_cast<Widget &&>(value));
  (void)dependentAlias(address);
  (void)deducedAlias(address);
  Writer writer = writeWidget;
  (void)valueStream(writer, value, 1UL);
  parameterPack(value, 1);
  (void)Nested<Widget>::instantiate(value);
  return identity(value);
}

} // namespace b004
