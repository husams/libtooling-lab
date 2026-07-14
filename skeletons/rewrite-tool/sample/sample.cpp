// Sample translation unit for AST exploration. Deliberately dense: it packs
// namespaces, classes, inheritance, templates, lambdas, macros, and a mix of
// statement kinds into a small file.
#include <string>
#include <vector>

#define SQUARE(x) ((x) * (x))
#define GREETING "hello"

namespace geo {

constexpr double kPi = 3.14159265358979;

struct Point {
  double x = 0.0;
  double y = 0.0;
};

class Shape {
public:
  explicit Shape(std::string name) : name_(std::move(name)) {}
  virtual ~Shape() = default;
  virtual double area() const = 0;
  const std::string &name() const { return name_; }

private:
  std::string name_;
};

class Circle : public Shape {
public:
  Circle(Point center, double radius)
      : Shape("circle"), center_(center), radius_(radius) {}
  double area() const override { return kPi * SQUARE(radius_); }

private:
  Point center_;
  double radius_;
};

template <typename T>
T clamp(T value, T lo, T hi) {
  if (value < lo)
    return lo;
  return value > hi ? hi : value;
}

} // namespace geo

enum class Color { Red, Green, Blue };

using ShapeList = std::vector<geo::Shape *>;

static double totalArea(const ShapeList &shapes) {
  double total = 0.0;
  for (const auto *shape : shapes)
    total += shape->area();
  return total;
}

int main() {
  geo::Circle unit({0.0, 0.0}, 1.0);
  ShapeList shapes{&unit};

  auto doubled = [](double v) { return 2 * v; };
  double result = doubled(totalArea(shapes));

  int level = geo::clamp(42, 0, 10);
  Color c = Color::Blue;
  (void)result;
  (void)level;
  (void)c;
  return 0;
}
