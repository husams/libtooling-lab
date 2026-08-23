#ifndef FACTS_TOOL_STORAGE_ITLIB_GENERATOR_H
#define FACTS_TOOL_STORAGE_ITLIB_GENERATOR_H

#include "storage/Generator.h"

#include <itlib/generator.hpp>

#include <utility>

namespace facts::storage::detail {

template <typename Value>
itlib::generator<Value> toItlibGenerator(Generator<Value> source) {
  for (auto &&value : source) {
    co_yield std::move(value);
  }
}

} // namespace facts::storage::detail

#endif
