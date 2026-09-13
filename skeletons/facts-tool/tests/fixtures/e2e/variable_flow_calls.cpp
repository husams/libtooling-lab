namespace variable_flow {

int produce(int value) {
  int output = value + 1;
  return output;
}

int helper(int value) {
  int local = produce(value);
  return local;
}

int update_reference(int &value) {
  value += produce(value);
  return value;
}

int copy_value(int value) {
  int copied = value;
  copied += 1;
  return copied;
}

int recursive(int value) {
  int current = value;
  if (value > 0) {
    current = recursive(value - 1);
  }
  return current;
}

void noise(int) {}

} // namespace variable_flow
