#pragma once

namespace e2e {

struct Widget;
union Payload;
class Policy;
class Deferred;

struct Widget {
  int value;
};

union Payload {
  int integral;
  double fractional;
};

class Policy {
public:
  int multiplier = 1;
};

struct PublicWidget : Widget {};

class PrivateWidget : Widget {};

class CompositeWidget : public Widget, protected virtual Policy {};

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
