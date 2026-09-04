// Intentionally omit <optional>; supply it with -include optional.
std::optional<int> maybe_value(bool enabled) {
  return enabled ? std::optional<int>{42} : std::nullopt;
}

int main() {
  return maybe_value(true).value_or(0) == 42 ? 0 : 1;
}
