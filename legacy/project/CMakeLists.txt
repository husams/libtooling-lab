# project/CMakeLists.txt — the sample multi-file app the capstone analyzes.
# We don't link against LLVM here; this is ordinary C++. The point is to
# generate a compile_commands.json so the capstone tool knows how to parse
# each file (include paths, C++ standard, etc.).
#
# Generate the compilation database:
#   cmake -G Ninja -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON .
#   # build/compile_commands.json now exists; point the tool at it with -p build
cmake_minimum_required(VERSION 3.20)
project(sample_app CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(widget src/widget.cpp)
target_include_directories(widget PUBLIC include)

add_executable(app src/main.cpp)
target_link_libraries(app PRIVATE widget)
