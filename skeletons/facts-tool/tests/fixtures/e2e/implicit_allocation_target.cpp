int *seed_new() { return new int(7); }

void *explicit_new(decltype(sizeof(0)) bytes) { return ::operator new(bytes); }
