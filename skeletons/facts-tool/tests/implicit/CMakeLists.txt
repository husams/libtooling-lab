add_executable(implicit-storage-test StorageInvariants.cpp)
target_link_libraries(implicit-storage-test PRIVATE facts-storage facts-model)
add_test(NAME implicit-storage COMMAND implicit-storage-test
  ${CMAKE_BINARY_DIR}/implicit-storage)
add_executable(implicit-declaration-test DeclarationResolution.cpp)
target_link_libraries(implicit-declaration-test PRIVATE facts-ast facts-storage facts-clang)
add_test(NAME implicit-declaration COMMAND implicit-declaration-test
  ${CMAKE_BINARY_DIR}/implicit-declarations)
add_executable(implicit-target-probe TargetProbe.cpp)
target_link_libraries(implicit-target-probe PRIVATE facts-ast facts-clang facts-storage)
add_executable(implicit-compiler-record-test CompilerRecordTarget.cpp)
target_link_libraries(implicit-compiler-record-test PRIVATE facts-ast facts-clang facts-storage)
add_test(NAME implicit-compiler-record COMMAND implicit-compiler-record-test
  ${CMAKE_BINARY_DIR}/implicit-compiler-record)
set_target_properties(implicit-storage-test implicit-declaration-test implicit-target-probe
  implicit-compiler-record-test
  PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
