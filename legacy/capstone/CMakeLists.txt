# capstone/CMakeLists.txt — builds the cpp-modernize linter.
#
# Configure (on the VM):
#   cmake -G Ninja -B build \
#     -DCMAKE_PREFIX_PATH=/usr/lib/llvm-19 \
#     -DCMAKE_CXX_COMPILER=clang++-19 .
#   cmake --build build
cmake_minimum_required(VERSION 3.20)
project(cpp_modernize CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)
message(STATUS "Building against LLVM ${LLVM_PACKAGE_VERSION}")

include_directories(SYSTEM ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})
if(NOT LLVM_ENABLE_RTTI)
  add_compile_options(-fno-rtti)
endif()

add_executable(cpp-modernize cpp-modernize.cpp)
target_link_libraries(cpp-modernize PRIVATE clang-cpp LLVM)
