using Callback = int (*)(double, const char *);
int local_target(double, const char *) { return 1; }
int alternative(double, const char *) { return 2; }
int external_target(double, const char *);
Callback factory();
Callback global_callback;

struct Receiver {
    Callback callback;
    int method(double) const { return 3; }
};

int parameter(Callback fp) {
    return fp(1.0, "parameter"); // pointer parameter
}
int known_local() {
    Callback fp = local_target;
    return fp(2.0, "local"); // pointer known_local
}
int reassigned(bool choose) {
    Callback fp = local_target;
    if (choose) fp = alternative;
    return fp(3.0, "reassigned"); // pointer reassigned
}
int field(Receiver &receiver) {
    return receiver.callback(4.0, "field"); // pointer field
}
int global() {
    return global_callback(5.0, "global"); // pointer global
}
int array(Callback *callbacks, unsigned index) {
    return callbacks[index](6.0, "array"); // pointer array
}
int factory_expression() {
    return factory()(7.0, "factory"); // pointer factory_expression
}
int conditional(bool choose, Callback left, Callback right) {
    return (choose ? left : right)(8.0, "conditional"); // pointer conditional
}
int member_pointer(Receiver &receiver, int (Receiver::*method)(double) const) {
    return (receiver.*method)(9.0); // pointer member_pointer
}
int variadic(int (*fp)(const char *, ...)) {
    return fp("variadic", 10); // pointer variadic
}
int nonthrowing(int (*fp)(double) noexcept) {
    return fp(11.0); // pointer nonthrowing
}
int function_reference(int (&fp)(double, const char *)) {
    return fp(12.0, "reference"); // pointer function_reference
}
int known_external() {
    return external_target(13.0, "external");
}
int reachable_chain(Callback fp) {
    return parameter(fp) + known_local();
}
