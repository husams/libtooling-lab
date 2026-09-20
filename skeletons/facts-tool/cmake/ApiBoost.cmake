set(_facts_boost_explicit FALSE)
if(FETCHCONTENT_SOURCE_DIR_BOOST)
  facts_header_root("${FETCHCONTENT_SOURCE_DIR_BOOST}" boost/version.hpp _facts_boost_include)
  set(_facts_boost_explicit TRUE)
elseif(FACTS_BOOST_INCLUDE_DIR)
  set(_facts_boost_include "${FACTS_BOOST_INCLUDE_DIR}")
  set(_facts_boost_explicit TRUE)
else()
  # CONFIG avoids the deprecated FindBoost module (CMP0167 in CMake 3.30+).
  find_package(Boost 1.74 QUIET CONFIG)
  if(TARGET Boost::headers)
    get_target_property(_facts_boost_hints Boost::headers INTERFACE_INCLUDE_DIRECTORIES)
  endif()
  unset(_facts_boost_path CACHE)
  find_path(_facts_boost_path boost/version.hpp
    HINTS ${_facts_boost_hints} ${Boost_INCLUDE_DIR} ${Boost_INCLUDE_DIRS}
      ${Boost_ROOT} ${BOOST_ROOT} ENV Boost_ROOT ENV BOOST_ROOT
    PATH_SUFFIXES include)
  set(_facts_boost_include "${_facts_boost_path}")
  unset(_facts_boost_path CACHE)
endif()

facts_boost_version("${_facts_boost_include}" _facts_boost_version)
if(_facts_boost_version LESS 107400 AND NOT _facts_boost_explicit
    AND NOT TARGET Boost::headers)
  message(STATUS "Boost >= 1.74 headers unavailable; fetching pinned Boost 1.83.0")
  FetchContent_Declare(Boost
    URL https://archives.boost.io/release/1.83.0/source/boost_1_83_0.tar.bz2
    URL_HASH SHA256=6478edfe2f3305127cffe8caf73ea0176c53769f4bf1585be237eb30798c3b8e
    # Populate only: Asio/Beast need headers, not a Boost library build.
    SOURCE_SUBDIR __facts_headers_only__)
  FetchContent_MakeAvailable(Boost)
  facts_header_root("${boost_SOURCE_DIR}" boost/version.hpp _facts_boost_include)
  facts_boost_version("${_facts_boost_include}" _facts_boost_version)
endif()

if(_facts_boost_version LESS 107400)
  message(FATAL_ERROR "Boost >= 1.74 Asio/Beast headers are required; check "
    "FETCHCONTENT_SOURCE_DIR_BOOST or FACTS_BOOST_INCLUDE_DIR")
endif()
set(FACTS_BOOST_INCLUDE_DIR "${_facts_boost_include}")
if(NOT TARGET Boost::headers)
  add_library(Boost::headers INTERFACE IMPORTED GLOBAL)
  set_target_properties(Boost::headers PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${FACTS_BOOST_INCLUDE_DIR}")
endif()
message(STATUS "API Boost headers: ${FACTS_BOOST_INCLUDE_DIR} (${_facts_boost_version})")
