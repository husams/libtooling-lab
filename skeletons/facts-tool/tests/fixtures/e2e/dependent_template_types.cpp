namespace b004 {

struct Widget {};

namespace support {
struct Marker {};
} // namespace support

using namespace support;

inline constexpr auto migrationSql = "ALTER TABLE symbol";
using Writer = bool (*)(const Widget &, unsigned long);

template <typename T>
T identity(T value) {
  return value;
}

template <>
Widget identity(Widget value) {
  return value;
}

template <typename T>
T *pointer(T *value) {
  return value;
}

template <typename T>
const T &lvalue(const T &value) {
  return value;
}

template <typename T>
T &&rvalue(T &&value) {
  return static_cast<T &&>(value);
}

template <typename Value>
auto dependentAlias(Value &&value) {
  using Owned = decltype(identity(*value));
  return Owned{};
}

template <typename Value>
auto ownBind(Value value) {
  return value;
}

template <typename Value>
auto deducedAlias(Value *value) {
  using Owned = decltype(ownBind(*value));
  return Owned{};
}

template <typename Map, typename... Values>
auto valueStream(Map map, Values... values) -> decltype(map(values...)) {
  return map(values...);
}

bool writeWidget(const Widget &, unsigned long) { return true; }

template <typename... Values>
void parameterPack(Values &&...values) {
  ((void)values, ...);
}

template <typename T>
struct Nested {
  template <typename U>
  static U relay(U value) {
    return value;
  }

  static T instantiate(T value) { return relay<T>(value); }
};

Widget instantiate(Widget value) {
  auto *address = &value;
  (void)identity(1);
  (void)pointer(address);
  (void)lvalue(value);
  (void)rvalue(static_cast<Widget &&>(value));
  (void)dependentAlias(address);
  (void)deducedAlias(address);
  Writer writer = writeWidget;
  (void)valueStream(writer, value, 1UL);
  parameterPack(value, 1);
  (void)Nested<Widget>::instantiate(value);
  return identity(value);
}

} // namespace b004

namespace b018 {

struct Concrete {};

struct Owner {
  Concrete value;
};

inline Concrete concreteValue;
inline constexpr int committedCanary = 18;

template <typename T>
struct DependentOwners {
  using DependentNameAlias = typename T::type;
  using DependentTemplateAlias = typename T::template rebind<Concrete>;

  typename T::type dependentNameField;
  typename T::template rebind<Concrete> dependentTemplateField;

  static typename T::type dependentNameRoundTrip(typename T::type value);
  static typename T::template rebind<Concrete>
  dependentTemplateRoundTrip(typename T::template rebind<Concrete> value);
  static void mixedParameters(typename T::type dependentFirst, int plainSecond,
                              Concrete plainThird);
};

using MemberPointerAlias = Concrete Owner::*;
using MemberPointerFunction = Concrete Owner::*(*)(Concrete Owner::*);
using ComplexAlias = _Complex double;
using AtomicAlias = _Atomic(Concrete);
using BlockPointerAlias = Concrete (^)(Concrete);
using TypeOfExprAlias = __typeof__(concreteValue);
using VectorAlias = int __attribute__((vector_size(16)));

struct WrapperFields {
  Concrete Owner::*memberPointerField;
  _Complex double complexField;
  _Atomic(Concrete) atomicField;
  Concrete (^blockPointerField)(Concrete);
  __typeof__(concreteValue) typeOfExprField;
  int vectorField __attribute__((vector_size(16)));
};

Concrete Owner::*memberPointerRoundTrip(Concrete Owner::*value);
_Complex double complexRoundTrip(_Complex double value);
_Atomic(Concrete) atomicRoundTrip(_Atomic(Concrete) value);
Concrete (^blockPointerRoundTrip(Concrete (^value)(Concrete)))(Concrete);
__typeof__(concreteValue) typeOfExprRoundTrip(__typeof__(concreteValue) value);
VectorAlias vectorRoundTrip(VectorAlias value);

} // namespace b018
