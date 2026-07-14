# tools/CMakeLists.txt — builds every per-part LibTooling tool in this lab.
# One CMake setup, reused for all five executables.
#
# Configure (on the VM):
#   cmake -G Ninja -B build \
#     -DCMAKE_PREFIX_PATH=/usr/lib/llvm-19 \
#     -DCMAKE_CXX_COMPILER=clang++-19 .
#   cmake --build build
cmake_minimum_required(VERSION 3.20)
project(libtooling_lab CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# find_package picks up the dirs under CMAKE_PREFIX_PATH (.../lib/cmake/{llvm,clang}).
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)
message(STATUS "Building against LLVM ${LLVM_PACKAGE_VERSION}")

include_directories(SYSTEM ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# LLVM/Clang are usually built without RTTI; your tool must match or you get
# obscure link/runtime errors.
if(NOT LLVM_ENABLE_RTTI)
  add_compile_options(-fno-rtti)
endif()

# Helper: declare a tool that links against the monolithic clang + LLVM libs.
# clang-cpp pulls in clangTooling/clangASTMatchers/clangRewrite/etc.; LLVM is
# the shared libLLVM. If your install lacks these targets, swap in the granular
# component libs (clangTooling clangASTMatchers ... LLVMSupport).
function(add_clang_tool name)
  add_executable(${name} ${ARGN})
  target_link_libraries(${name} PRIVATE clang-cpp LLVM)
endfunction()

add_clang_tool(firsttool   firsttool.cpp)
add_clang_tool(declprinter declprinter.cpp)
add_clang_tool(matchfns    matchfns.cpp)
add_clang_tool(injectlog   injectlog.cpp)
add_clang_tool(fixnull     fixnull.cpp)
