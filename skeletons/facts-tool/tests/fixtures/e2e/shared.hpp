#pragma once

namespace e2e {

struct Widget;
union Payload;
class Policy;
class Deferred;

struct Widget {
  int value;
};

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

int transform(const Widget &widget, Count factor = 2);
void userDefinedTypes(Widget value, Widget *pointer, Widget &reference,
                      Widget values[], Mode mode, Count count, Payload payload,
                      Policy policy);
void primitiveTypes(int signedValue, bool enabled, double ratio,
                    const char *text, void *payload);

} // namespace e2e
