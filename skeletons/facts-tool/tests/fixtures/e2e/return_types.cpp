namespace return_types {
struct Widget { int value; };

int free_function() { return 42; }
void free_void() {}
Widget free_value() { return {7}; }
Widget* free_pointer(Widget& value) { return &value; }
const Widget& free_reference(const Widget& value) { return value; }
auto trailing_return() -> Widget { return {8}; }

struct Service {
    Widget value{9};
    const Widget& method() const { return value; }
    static int static_method() { return 10; }
};

struct FunctionObject {
    Widget operator()() const { return {11}; }
    int operator()(int value) const { return value; }
};

int exercise() {
    Widget value{12};
    Service service;
    FunctionObject callable;
    auto explicit_lambda = [](Widget& item) -> Widget* { return &item; };
    auto deduced_lambda = [] { return 13; };
    free_void();
    return free_function() + free_value().value
        + free_pointer(value)->value + free_reference(value).value
        + trailing_return().value + service.method().value
        + Service::static_method() + callable().value + callable(14)
        + explicit_lambda(value)->value + deduced_lambda();
}
}
