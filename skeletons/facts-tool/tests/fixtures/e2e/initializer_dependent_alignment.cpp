namespace b020 {

struct alignas(8) AlignFixture {
  char storage[8];
};

template <typename T>
struct DependentMember {
  static constexpr unsigned long value = alignof(T);
};

template <typename T>
constexpr unsigned long dependentVariable = alignof(T);

template <typename T, unsigned long N>
constexpr unsigned long valueDependentVariable = N + sizeof(T);

constexpr unsigned long concreteSize = sizeof(AlignFixture);
constexpr unsigned long concreteAlignment = alignof(AlignFixture);

template struct DependentMember<AlignFixture>;
constexpr unsigned long instantiatedMember =
    DependentMember<AlignFixture>::value;

} // namespace b020
