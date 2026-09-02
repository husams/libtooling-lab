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

void caller() {
  std::string value = "hello";
  print(value);
  variable += 1;
}

} // namespace targeted_match
