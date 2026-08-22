namespace b004 {

struct Widget {};

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

Widget instantiate(Widget value) {
  auto *address = &value;
  (void)identity(1);
  (void)pointer(address);
  (void)lvalue(value);
  (void)rvalue(static_cast<Widget &&>(value));
  return identity(value);
}

} // namespace b004
