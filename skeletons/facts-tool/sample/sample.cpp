// sample.cpp — the translation unit facts-tool reads.
//
// Deliberately small and varied: a namespace, a class with members, a free
// function template, and a call site, so each new kind of fact has something
// to find here as the tool grows.

#include <string>
#include <vector>

namespace geo {

struct Point {
  double X = 0;
  double Y = 0;

  double lengthSquared() const { return X * X + Y * Y; }
};

class Shape {
public:
  explicit Shape(std::string Name) : Name(std::move(Name)) {}
  virtual ~Shape() = default;

  virtual double area() const = 0;
  const std::string &name() const { return Name; }

private:
  std::string Name;
};

class Circle final : public Shape {
public:
  Circle(Point Center, double Radius)
      : Shape("circle"), Center(Center), Radius(Radius) {}

  double area() const override { return 3.14159265358979 * Radius * Radius; }

private:
  Point Center;
  double Radius;
};

template <typename T> T sum(const std::vector<T> &Values) {
  T Total{};
  for (const T &V : Values)
    Total += V;
  return Total;
}

} // namespace geo

int main() {
  geo::Circle C({1.0, 2.0}, 3.0);
  std::vector<double> Areas{C.area()};
  return static_cast<int>(geo::sum(Areas)) & 0;
}
