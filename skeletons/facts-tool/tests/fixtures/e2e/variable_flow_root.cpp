namespace variable_flow {

int produce(int value);
int helper(int value);
int update_reference(int &value);
int copy_value(int value);
int recursive(int value);
int external_value(int value);
void noise(int value);

int root(int seed) {
  int result = helper(seed);
  update_reference(result);
  int copy = copy_value(result);
  int unrelated = 99;
  noise(unrelated);
  if (result > 0) {
    result += produce(copy);
  } else {
    result = copy;
  }
  for (int i = 0; i < 2; ++i) {
    result = result + i;
  }
  return result;
}

int shadowed(int value) {
  int result = value;
  {
    int value = result + 1;
    result = value;
  }
  return result;
}

int external_boundary(int seed) { return external_value(seed); }

int indirect_boundary(int seed) {
  int (*function)(int) = produce;
  return function(seed);
}

int depth_root(int seed) {
  int result = produce(seed);
  return result;
}

int repeated_helper(int seed) {
  int result = helper(seed);
  result = helper(result);
  return result;
}

} // namespace variable_flow
