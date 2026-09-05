template <typename T> struct Holder {
  T value{};
  void flush() {}
};

template struct Holder<int>;

enum Color { Red };

int save() { return 1; }

int run() {
  Holder<int> holder;
  holder.flush();
  return save() + save() + holder.value;
}
