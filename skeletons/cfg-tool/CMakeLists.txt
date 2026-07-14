# cfg-tool — skeleton for a clang/Analysis (CFG & call graph) tool.
#
# Configure & build (macOS, Homebrew LLVM):
#   cmake -G Ninja -B build \
#     -DCMAKE_PREFIX_PATH=$(brew --prefix llvm) \
#     -DCMAKE_CXX_COMPILER=$(brew --prefix llvm)/bin/clang++ .
#   cmake --build build
cmake_minimum_required(VERSION 3.20)
# C is needed too: LLVM's own cmake modules (FindLibEdit) run C header checks.
project(cfg_tool C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)
message(STATUS "Building against LLVM ${LLVM_PACKAGE_VERSION} at ${LLVM_DIR}")

include_directories(SYSTEM ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# LLVM is usually built without RTTI; the tool must match or linking fails.
if(NOT LLVM_ENABLE_RTTI)
  add_compile_options(-fno-rtti)
endif()

add_executable(cfg-tool main.cpp)
# clang-cpp is the monolithic Clang dylib; LLVM is the monolithic LLVM dylib.
target_link_libraries(cfg-tool PRIVATE clang-cpp LLVM)
