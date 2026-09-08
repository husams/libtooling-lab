namespace expression_evidence {

using RefFn = void (*)(int &);

void consume(int &) {}

struct Base {
  virtual ~Base() = default;
};

struct Record : Base {
  int field = 0;
  void write() { field = 1; }
  void overload(int) { field = 2; }
  void overload(double) { field = 3; }
  void outOfLine();
  void update() { field += 1; }
  int read() const { return field; }
  void escape(int *out) { out = &field; }
  void reference() { consume(field); }
  void unknown(RefFn fn) { fn(field); }
  struct Nested {
    int value = 0;
    void set() { value = 1; }
  };
};

inline void Record::outOfLine() { field = 4; }

template <typename T>
struct Box {
  T value;
  void set(T v) { value = v; }
};

struct Derived : Record {};

void declaredOnly();

#define DEFINE_MACRO_FUNCTION(name) void name() {}
DEFINE_MACRO_FUNCTION(macroDefined)

} // namespace expression_evidence
