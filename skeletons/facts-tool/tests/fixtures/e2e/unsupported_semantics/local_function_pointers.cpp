void target() {}

void caller() {
    void (*fp)() = target;
    fp();
}

void address_target() {
    void (*fp)() = &target;
    (*fp)();
}

void parenthesized_target() {
    auto fp = target;
    (fp)();
}
