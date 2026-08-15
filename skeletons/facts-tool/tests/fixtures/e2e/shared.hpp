#pragma once

namespace e2e {

struct Widget;
union Payload;
class Policy;
class Deferred;

template <typename T, template <typename> class Container>
class ClassTemplate {};

template <typename T, int N>
struct StructTemplate {};

template <typename T, int N>
struct StructTemplate<T *, N> {};

template <>
struct StructTemplate<Policy, 3> {};

template <typename... Ts>
union UnionTemplate {};

template <typename T, int N>
int functionTemplate() {
  return N;
}

template <>
inline int functionTemplate<Policy, 5>() {
  return 5;
}

class MethodTemplateFixture {
public:
  template <typename T>
  int methodTemplate() const {
    return sizeof(T);
  }
};

template <>
inline int MethodTemplateFixture::methodTemplate<Policy>() const {
  return 5;
}

struct Widget {
  int value;
};

struct ConstClass {
  constexpr explicit ConstClass(int value) : value(value) {}

  int value;
};

struct InitializerFixture {
  int count = 2 + 3;
  bool enabled = true;
  const char *label = "ready";
  int values[3] = {1, 2, 3};
  static constexpr int limit = 6 * 7;
  static const char *name;
};

extern int mergedGlobal;
inline constexpr int inlineGlobal = 14;

typedef Widget WidgetTypedef;
using WidgetAlias = Widget;

template <int N>
using StructAlias = StructTemplate<Widget, N>;

class MethodFixture {
public:
  virtual int inlineMethod() const { return 1; }

  virtual int pureMethod() const = 0;
  void deletedMethod() = delete;
  bool operator==(const MethodFixture &) const = default;
  int outOfLineMethod(int value) const;
};

struct MyRecord {
  int s;
};

union Payload {
  int integral;
  double fractional;
};

class Policy {
public:
  virtual int apply() const { return multiplier; }

  int multiplier = 1;
};

struct PublicWidget : Widget {};

class PrivateWidget : Widget {};

class CompositeWidget : public Widget, protected virtual Policy {
public:
  int apply() const override { return multiplier; }
};

enum class Mode { Fast, Slow };

using Count = int;

extern int sharedCounter;

inline int headerHelper(int input, int delta = 1) { return input + delta; }

void defaultArguments(int count = 2 + 3, bool enabled = true,
                      const char *label = "ready", Widget widget = Widget{7});

int transform(const Widget &widget, Count factor = 2);
void userDefinedTypes(Widget value, Widget *pointer, Widget &reference,
                      Widget values[], Mode mode, Count count, Payload payload,
                      Policy policy);
void primitiveTypes(int signedValue, bool enabled, double ratio,
                    const char *text, void *payload);

} // namespace e2e
