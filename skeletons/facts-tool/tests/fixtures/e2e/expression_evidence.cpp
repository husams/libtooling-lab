namespace expression_evidence {

struct Base {
  virtual ~Base() = default;
};

struct Record : Base {
  int field = 0;
  void write() { field = 1; }
  void update() { field += 1; }
  int read() const { return field; }
  void escape(int *out) { out = &field; }
};

template <typename T>
struct Box {
  T value;
  void set(T v) { value = v; }
};

struct Derived : Record {};

} // namespace expression_evidence
