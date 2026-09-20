set(_facts_json_explicit FALSE)
if(FETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON)
  facts_header_root("${FETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON}" nlohmann/json.hpp _facts_json_include)
  set(_facts_json_explicit TRUE)
elseif(FACTS_JSON_INCLUDE_DIR)
  set(_facts_json_include "${FACTS_JSON_INCLUDE_DIR}")
  set(_facts_json_explicit TRUE)
else()
  find_package(nlohmann_json 3.9 QUIET CONFIG)
  if(TARGET nlohmann_json::nlohmann_json)
    get_target_property(_facts_json_hints nlohmann_json::nlohmann_json INTERFACE_INCLUDE_DIRECTORIES)
  endif()
  unset(_facts_json_path CACHE)
  find_path(_facts_json_path nlohmann/json.hpp HINTS ${_facts_json_hints})
  set(_facts_json_include "${_facts_json_path}")
  unset(_facts_json_path CACHE)
endif()

facts_json_version("${_facts_json_include}" _facts_json_version)
if(_facts_json_version VERSION_LESS 3.9 AND NOT _facts_json_explicit
    AND NOT TARGET nlohmann_json::nlohmann_json)
  message(STATUS "nlohmann_json >= 3.9 headers unavailable; fetching pinned 3.11.3")
  FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
    SOURCE_SUBDIR __facts_headers_only__)
  FetchContent_MakeAvailable(nlohmann_json)
  facts_header_root("${nlohmann_json_SOURCE_DIR}" nlohmann/json.hpp _facts_json_include)
  facts_json_version("${_facts_json_include}" _facts_json_version)
endif()

if(_facts_json_version VERSION_LESS 3.9)
  message(FATAL_ERROR "nlohmann_json >= 3.9 headers are required; check "
    "FETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON or FACTS_JSON_INCLUDE_DIR")
endif()
set(FACTS_JSON_INCLUDE_DIR "${_facts_json_include}")
if(NOT TARGET nlohmann_json::nlohmann_json)
  add_library(nlohmann_json::nlohmann_json INTERFACE IMPORTED GLOBAL)
  set_target_properties(nlohmann_json::nlohmann_json PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${FACTS_JSON_INCLUDE_DIR}"
    INTERFACE_COMPILE_FEATURES cxx_std_11)
endif()
message(STATUS "API nlohmann_json headers: ${FACTS_JSON_INCLUDE_DIR} (${_facts_json_version})")
