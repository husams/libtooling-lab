namespace b0xx {

template <typename T>
struct Holder {
  T value;
};

struct Widget {};

struct Policy {};

struct Canary {
  int seen;
};

Holder<Widget> *pointerOnly = nullptr;
Holder<int> &referenceOnly(Holder<int> &in);
using AliasOnly = Holder<double> *;

struct Owner {
  Holder<char> *field;
};

Holder<Policy> instantiated{};

Canary canary{0};

} // namespace b0xx
