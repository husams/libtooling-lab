#ifndef S022_FIXTURE_SERVICE_HPP
#define S022_FIXTURE_SERVICE_HPP

namespace s022_fixture {

int helper();

struct Base {
  virtual ~Base();
  virtual int value() const;
};

struct Derived final : Base {
  Derived();
  ~Derived() override;
  int value() const override;
};

int invoke(Base *value);

} // namespace s022_fixture

#endif
