void target() {}
void other() {}
void mutate(void (**fp)());

void parameter(void (*fp)()) {
    fp(); // unresolved
}

void reassigned() {
    void (*fp)() = target;
    fp = other;
    fp(); // unresolved
}

void escaped() {
    void (*fp)() = target;
    mutate(&fp);
    fp(); // unresolved
}

void aliased() {
    void (*fp)() = target;
    auto &alias = fp;
    alias = other;
    fp(); // unresolved
}

void captured() {
    void (*fp)() = target;
    auto change = [&fp] { fp = other; };
    change();
    fp(); // unresolved
}
