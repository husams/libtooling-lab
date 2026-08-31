#include "call_graph.hpp"

namespace call_graph_fixture {

int Base::toString() const { return 0; }

int Base::log() const { return toString(); }

int X::toString() const { return 1; }

int Y::toString() const { return 2; }

int templateCalls(X &x, Y &y) { return invoke(x) + invoke(y); }

} // namespace call_graph_fixture
