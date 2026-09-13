namespace b022 {

void sibling() {}

void canary() {
  __builtin_va_list list{};
  auto copy = list[0];
  sibling();
  (void)copy;
}

auto returns_builtin() {
  __builtin_va_list list{};
  return list[0];
}

} // namespace b022
