#pragma once

namespace external {

class Base {
public:
  virtual int externalMethod() const { return 7; }

private:
  int externalState = 7;
};

} // namespace external
