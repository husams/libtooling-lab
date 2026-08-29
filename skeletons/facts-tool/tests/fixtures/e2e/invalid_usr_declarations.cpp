namespace probe {

struct Bits {
  unsigned named : 3;
  unsigned : 0;
};

struct Canary {
  int seen;
};

Bits bits{};
Canary canary{0};

} // namespace probe
