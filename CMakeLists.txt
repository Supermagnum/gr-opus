cmake_minimum_required(VERSION 3.8)
project(gr-opus)

# Set the version of this module here
set(GR_OPUS_VERSION_MAJOR 1)
set(GR_OPUS_VERSION_MINOR 0)
set(GR_OPUS_VERSION_PATCH 0)
set(GR_OPUS_VERSION "${GR_OPUS_VERSION_MAJOR}.${GR_OPUS_VERSION_MINOR}.${GR_OPUS_VERSION_PATCH}")

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(GR REQUIRED gnuradio-runtime)

# Find Opus library
pkg_check_modules(OPUS REQUIRED opus)

# Include directories
include_directories(${GR_INCLUDE_DIRS})
include_directories(${OPUS_INCLUDE_DIRS})

# Set Python installation paths
if(NOT GR_PYTHON_DIR)
    execute_process(
        COMMAND python3 -c "import gnuradio; import os; print(os.path.dirname(gnuradio.__file__))"
        OUTPUT_VARIABLE GR_PYTHON_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif()
set(GR_PYTHON_DIR ${GR_PYTHON_DIR} CACHE PATH "Directory for Python modules")

if(NOT GR_INCLUDE_DIR)
    set(GR_INCLUDE_DIR ${CMAKE_INSTALL_PREFIX}/include CACHE PATH "GNU Radio include directory")
endif()

if(NOT GR_LIBRARY_DIR)
    set(GR_LIBRARY_DIR ${CMAKE_INSTALL_PREFIX}/lib CACHE PATH "GNU Radio library directory")
endif()

if(NOT GRC_BLOCKS_DIR)
    # Default to /usr/local/share to match GNU Radio source installs
    set(GRC_BLOCKS_DIR "${CMAKE_INSTALL_PREFIX}/share/gnuradio/grc/blocks")
endif()
set(GRC_BLOCKS_DIR ${GRC_BLOCKS_DIR} CACHE PATH "Directory for GRC block definitions")

# Add subdirectories
add_subdirectory(lib)

# Check for SWIG (optional - for Python bindings)
find_package(SWIG)
if(SWIG_FOUND)
    add_subdirectory(swig)
    message(STATUS "SWIG found - Python bindings will be built")
else()
    message(STATUS "SWIG not found - Python bindings will be skipped (using Python fallback)")
    message(STATUS "  To enable Python bindings, install SWIG: sudo apt-get install swig")
endif()

add_subdirectory(python)
add_subdirectory(grc)
add_subdirectory(tests)

