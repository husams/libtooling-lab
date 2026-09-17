struct Trivial { int value; };
struct Defaulted { Defaulted() = default; ~Defaulted() = default; };
struct Base { ~Base() {} };
struct Member { ~Member() {} };
struct ArrayElement { ~ArrayElement() {} };
struct Owner : Base {
    Member member;
    ArrayElement elements[2];
    ~Owner() = default;
};

void caller() {
    Trivial trivial{};
    Defaulted defaulted;
    Owner owner;
}

void temporary() { (void)Owner{}; }
