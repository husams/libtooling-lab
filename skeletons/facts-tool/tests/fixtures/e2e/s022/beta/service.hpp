#ifndef S022_FIXTURE_SERVICE_HPP
#define S022_FIXTURE_SERVICE_HPP

namespace s022_fixture {

int helper();

struct Base {
  virtual ~Base();
  virtual int value() const;
};

struct Member {
  ~Member();
};

struct Derived : Base {
  Derived();
  ~Derived() override;
  int value() const override;
  Member member;
};

struct ImplicitValue {
  int value = helper();
};

int invoke(Base *value);
int consume(ImplicitValue value);
inline int headerIndirect(int (*target)()) { return target(); }

} // namespace s022_fixture

#endif
