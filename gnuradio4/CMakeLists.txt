# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.22)
if(NOT DEFINED CMAKE_CXX_COMPILER)
  find_program(_GR_OPUS4_CXX_PROG NAMES g++-14 g++-15 DOC "C++23 compiler compatible with gnu-radio-core")
  if(_GR_OPUS4_CXX_PROG)
    set(CMAKE_CXX_COMPILER "${_GR_OPUS4_CXX_PROG}" CACHE FILEPATH "C++ compiler" FORCE)
    message(STATUS "gr-opus4: CMAKE_CXX_COMPILER set to ${_GR_OPUS4_CXX_PROG}")
  endif()
endif()
project(
  gr-opus4
  VERSION 0.1.0
  LANGUAGES CXX
  DESCRIPTION "GNU Radio 4.0 Opus audio codec blocks")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(GR_OPUS4_BUILD_TESTS "Build Boost.UT tests" ON)

find_package(gnuradio4 REQUIRED CONFIG)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PC_OPUS REQUIRED IMPORTED_TARGET opus)

include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_INCLUDES ${PC_OPUS_INCLUDE_DIRS})

check_cxx_source_compiles("
#include <opus/opus.h>
#include <opus/opus_defines.h>
#ifdef OPUS_SET_DRED_DURATION_REQUEST
int main() { return 0; }
#else
#error DRED not available
#endif
" GR_OPUS4_HAVE_OPUS_DRED)

set(CMAKE_REQUIRED_INCLUDES "")
set(CMAKE_REQUIRED_LIBRARIES "")

if(GR_OPUS4_HAVE_OPUS_DRED)
  message(STATUS "Opus: OPUS_SET_DRED_DURATION available (HAVE_OPUS_DRED)")
else()
  message(STATUS "Opus: built without OPUS_SET_DRED_DURATION; enable_fargan will be ignored")
endif()

add_library(gnuradio4-gr-opus INTERFACE)
add_library(gnuradio4::gr-opus ALIAS gnuradio4-gr-opus)
target_include_directories(
  gnuradio4-gr-opus
  INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>)

target_link_libraries(gnuradio4-gr-opus INTERFACE gnuradio4::gnuradio-core gnuradio4::gnuradio-blocklib-core PkgConfig::PC_OPUS)

if(GR_OPUS4_HAVE_OPUS_DRED)
  target_compile_definitions(gnuradio4-gr-opus INTERFACE HAVE_OPUS_DRED=1)
endif()

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

install(
  TARGETS gnuradio4-gr-opus
  EXPORT gr-opus4-targets
  INCLUDES
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

install(
  EXPORT gr-opus4-targets
  FILE gr-opus4-targets.cmake
  NAMESPACE gnuradio4::
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/gr-opus4")

include(CMakePackageConfigHelpers)

configure_package_config_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/gr-opus4-config.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/gr-opus4-config.cmake"
                              INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/gr-opus4")

write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/gr-opus4-config-version.cmake" VERSION "${PROJECT_VERSION}" COMPATIBILITY SameMajorVersion)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/gr-opus4-config.cmake" "${CMAKE_CURRENT_BINARY_DIR}/gr-opus4-config-version.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/gr-opus4")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/gnuradio4-gr-opus.pc.in" "${CMAKE_CURRENT_BINARY_DIR}/gnuradio4-gr-opus.pc"
               @ONLY)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/gnuradio4-gr-opus.pc" DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

if(GR_OPUS4_BUILD_TESTS)
  include(FetchContent)
  set(BOOST_UT_DISABLE_MODULE ON CACHE BOOL "Disable UT Module Support" FORCE)
  FetchContent_Declare(
    ut
    GIT_REPOSITORY https://github.com/boost-ext/ut.git
    GIT_TAG 53e17f25119598c6458d30351b260193096ba67e
    EXCLUDE_FROM_ALL)
  FetchContent_MakeAvailable(ut)
  enable_testing()
  add_subdirectory(test)
endif()
