add_test(NAME openapi-generated
  COMMAND ${Python3_EXECUTABLE}
    ${PROJECT_SOURCE_DIR}/scripts/generate_openapi.py --check)
set_tests_properties(openapi-generated PROPERTIES TIMEOUT 30)
add_test(NAME openapi-contract
  COMMAND ${Python3_EXECUTABLE} -m pytest
    ${CMAKE_CURRENT_SOURCE_DIR} -q)
set_tests_properties(openapi-contract PROPERTIES TIMEOUT 120)
