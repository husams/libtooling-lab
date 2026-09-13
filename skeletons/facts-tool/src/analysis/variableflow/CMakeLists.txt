add_library(facts-variableflow STATIC
  Engine.cpp
  Parse.cpp
  Identity.cpp
  Signature.cpp
  Flow.cpp
  Calls.cpp
  Effects.cpp
  GraphBuilder.cpp
  Access.cpp
  Expressions.cpp
  Assignments.cpp
  Statements.cpp
  CFGEvents.cpp
  Reaching.cpp
)
target_include_directories(facts-variableflow PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(facts-variableflow PUBLIC facts-clang PRIVATE facts-platform)
