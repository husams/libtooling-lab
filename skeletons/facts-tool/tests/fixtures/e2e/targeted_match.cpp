#include <string>

namespace targeted_match {

struct Base {
  virtual void run() {}
};

struct Record : Base {
  int field = 0;

  void run() override {}
};

enum Colour { Red };

int variable = 1;

template <typename T>
struct Box {};

template <typename T>
using Alias = T;

void print(std::string &value);
void print(int &value);
void sink(int &value);
void number(int value);

void caller() {
  std::string value = "hello";
  print(value);
  variable += 1;
}

struct OwnerCalls {
  OwnerCalls() {
    int value = 0;
    sink(value);
  }

  void method() {
    int value = 0;
    sink(value);
  }
};

void functionOwner() {
  int value = 0;
  sink(value);
}

void lambdaOwner() {
  auto nested = [] {
    int value = 0;
    sink(value);
  };
  nested();
}

void constantArgument() { number(42); }

void indirectTarget(int &value);

void indirectCaller() {
  auto *function = indirectTarget;
  int value = 0;
  function(value);
}

} // namespace targeted_match
