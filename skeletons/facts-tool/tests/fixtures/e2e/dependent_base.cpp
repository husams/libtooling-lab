namespace regression {

template <typename Base> struct DependentMixin : Base {
  int value = 0;
};

struct Concrete {
  int base = 1;
};

DependentMixin<Concrete> instance;

} // namespace regression
