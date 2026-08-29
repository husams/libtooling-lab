#include "forward_template_target_system.hpp"

namespace forward_target {

template <template <typename> class Template>
struct Envelope {};

Envelope<Late> before_definition{};

template <typename T>
struct Late {};

struct Canary {};

Canary canary{};

} // namespace forward_target
