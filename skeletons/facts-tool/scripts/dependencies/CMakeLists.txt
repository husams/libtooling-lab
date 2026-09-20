cmake_minimum_required(VERSION 3.20)
project(facts_api_header_dependencies LANGUAGES NONE)

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/ApiDependencies.cmake")

# Carry the resolved roots into the real build without repeating acquisition.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/api-header-cache.cmake"
  "set(FACTS_BOOST_INCLUDE_DIR [==[${FACTS_BOOST_INCLUDE_DIR}]==] CACHE PATH \"Boost headers\" FORCE)\n"
  "set(FACTS_JSON_INCLUDE_DIR [==[${FACTS_JSON_INCLUDE_DIR}]==] CACHE PATH \"nlohmann JSON headers\" FORCE)\n")
