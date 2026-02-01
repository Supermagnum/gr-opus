########################################################################
# Setup SWIG
########################################################################

# Set CMake policies for UseSWIG
if(POLICY CMP0078)
    cmake_policy(SET CMP0078 NEW)
endif()
if(POLICY CMP0086)
    cmake_policy(SET CMP0086 NEW)
endif()

find_package(SWIG)
if(NOT SWIG_FOUND)
    message(FATAL_ERROR "SWIG is required for Python bindings. Install SWIG or disable this subdirectory.")
endif()

include(UseSWIG)

if(${SWIG_VERSION} VERSION_GREATER_EQUAL 4.0.0)
    list(APPEND CMAKE_SWIG_FLAGS "-doxygen")
endif()

find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
find_package(PkgConfig REQUIRED)

########################################################################
# Setup SWIG flags
########################################################################

set(CMAKE_SWIG_FLAGS -fvirtual -python -c++ -keyword -w511)

# Initialize GR_SWIG_INCLUDE_DIRS
set(GR_SWIG_INCLUDE_DIRS "")

# Get GNU Radio prefix for swig path (same as GRC blocks)
set(GR_SWIG_SEARCH_PATHS
    ${GR_INCLUDE_DIRS}
    "${CMAKE_INSTALL_PREFIX}/share/gnuradio/swig"
    "/usr/local/share/gnuradio/swig"
    "/usr/share/gnuradio/swig"
    "${CMAKE_INSTALL_PREFIX}/include/gnuradio/swig"
    "/usr/local/include/gnuradio/swig"
    "/usr/include/gnuradio/swig"
)
execute_process(
    COMMAND pkg-config --variable=prefix gnuradio-runtime
    OUTPUT_VARIABLE GR_PREFIX_SWIG
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE GR_PREFIX_RESULT
)
if(GR_PREFIX_RESULT EQUAL 0 AND GR_PREFIX_SWIG)
    list(APPEND GR_SWIG_SEARCH_PATHS
        "${GR_PREFIX_SWIG}/share/gnuradio/swig"
        "${GR_PREFIX_SWIG}/include/gnuradio/swig"
    )
endif()

# Find gnuradio.i (required for SWIG interface)
# GNU Radio 3.9+ uses PyBind11 for core; OOT modules may need gnuradio.i from build
# or use the bundled minimal fallback in swig/gnuradio.i
find_file(GR_SWIG_I_FILE
    NAMES gnuradio.i
    PATHS ${GR_SWIG_SEARCH_PATHS}
    NO_DEFAULT_PATH
)

if(GR_SWIG_I_FILE)
    get_filename_component(GR_SWIG_I_DIR ${GR_SWIG_I_FILE} DIRECTORY)
    message(STATUS "Found gnuradio.i at: ${GR_SWIG_I_FILE}")
    list(APPEND CMAKE_SWIG_FLAGS "-I${GR_SWIG_I_DIR}")
    list(APPEND GR_SWIG_INCLUDE_DIRS ${GR_SWIG_I_DIR})
else()
    find_file(GR_SWIG_I_FILE NAMES gnuradio.i PATHS ${GR_SWIG_SEARCH_PATHS})
    if(GR_SWIG_I_FILE)
        get_filename_component(GR_SWIG_I_DIR ${GR_SWIG_I_FILE} DIRECTORY)
        message(STATUS "Found gnuradio.i at: ${GR_SWIG_I_FILE}")
        list(APPEND CMAKE_SWIG_FLAGS "-I${GR_SWIG_I_DIR}")
        list(APPEND GR_SWIG_INCLUDE_DIRS ${GR_SWIG_I_DIR})
    else()
        message(STATUS "gnuradio.i not in GNU Radio install - using bundled swig/gnuradio.i")
        message(STATUS "  (GNU Radio 3.9+ often omits SWIG interfaces; gr-opus provides a minimal copy)")
    endif()
endif()

########################################################################
# Find GNU Radio swig
########################################################################

# Try pkg-config first (for system installations)
pkg_check_modules(GR_SWIG gnuradio-swig)

# If found via pkg-config, preserve any gnuradio.i directories we found earlier
if(GR_SWIG_FOUND)
    # GR_SWIG_INCLUDE_DIRS is set by pkg_check_modules, but we want to keep
    # any directories we found for gnuradio.i
    set(GR_SWIG_INCLUDE_DIRS_TEMP ${GR_SWIG_INCLUDE_DIRS})
    set(GR_SWIG_INCLUDE_DIRS ${GR_SWIG_INCLUDE_DIRS_TEMP})
    # Add gnuradio.i directory if we found it
    if(GR_SWIG_I_DIR)
        list(APPEND GR_SWIG_INCLUDE_DIRS ${GR_SWIG_I_DIR})
    endif()
endif()

# If not found via pkg-config, try to find it manually (for source builds)
if(NOT GR_SWIG_FOUND)
    message(STATUS "gnuradio-swig not found via pkg-config, trying manual detection for source build...")
    
    # When GNU Radio is built from source, SWIG includes are usually in the same directory
    # Look for swig runtime header in common locations
    find_path(GR_SWIG_INCLUDE_DIR
        NAMES swig_runtime.h
        PATHS ${GR_INCLUDE_DIRS}
             "${CMAKE_INSTALL_PREFIX}/include"
             "/usr/local/include"
             "/usr/include"
        PATH_SUFFIXES gnuradio/swig
    )
    
    if(GR_SWIG_INCLUDE_DIR)
        message(STATUS "Found GNU Radio SWIG includes at: ${GR_SWIG_INCLUDE_DIR}")
        get_filename_component(GR_SWIG_BASE_DIR ${GR_SWIG_INCLUDE_DIR} DIRECTORY)
        list(APPEND GR_SWIG_INCLUDE_DIRS ${GR_SWIG_BASE_DIR})
        set(GR_SWIG_FOUND TRUE)
    else()
        # Fallback: use GR_INCLUDE_DIRS (SWIG headers should be there for source builds)
        message(STATUS "SWIG headers not found in standard locations, using GR_INCLUDE_DIRS: ${GR_INCLUDE_DIRS}")
        list(APPEND GR_SWIG_INCLUDE_DIRS ${GR_INCLUDE_DIRS})
        set(GR_SWIG_FOUND TRUE)
    endif()
    
    # Try to find the library (optional - modern GNU Radio may not need it)
    find_library(GR_SWIG_LIBRARY
        NAMES gnuradio-swig
        PATHS ${GR_LIBRARY_DIR}
             "${CMAKE_INSTALL_PREFIX}/lib"
             "/usr/local/lib"
             "/usr/lib"
    )
    
    if(GR_SWIG_LIBRARY)
        set(GR_SWIG_LIBRARIES ${GR_SWIG_LIBRARY})
        message(STATUS "Found GNU Radio SWIG library: ${GR_SWIG_LIBRARY}")
    else()
        # Library is optional for modern GNU Radio (header-only SWIG runtime)
        message(STATUS "GNU Radio SWIG library not found (this is OK for header-only runtime)")
        set(GR_SWIG_LIBRARIES "")
    endif()
endif()

if(NOT GR_SWIG_FOUND)
    message(FATAL_ERROR "gnuradio-swig package is required for Python bindings.")
    message(FATAL_ERROR "  For source builds, ensure GNU Radio SWIG headers are in GR_INCLUDE_DIRS")
    message(FATAL_ERROR "  Current GR_INCLUDE_DIRS: ${GR_INCLUDE_DIRS}")
endif()

message(STATUS "GNU Radio SWIG include dirs: ${GR_SWIG_INCLUDE_DIRS}")
if(GR_SWIG_LIBRARIES)
    message(STATUS "GNU Radio SWIG libraries: ${GR_SWIG_LIBRARIES}")
endif()

########################################################################
# Include directories
########################################################################

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/../include)
include_directories(${GR_SWIG_INCLUDE_DIRS})
include_directories(${GR_INCLUDE_DIRS})
include_directories(${OPUS_INCLUDE_DIRS})
include_directories(${Python3_INCLUDE_DIRS})

# Add include directories to SWIG flags
foreach(INCLUDE_DIR ${GR_SWIG_INCLUDE_DIRS} ${GR_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/../include)
    list(APPEND CMAKE_SWIG_FLAGS "-I${INCLUDE_DIR}")
endforeach()

# If gnuradio.i was not found, use our bundled minimal copy
if(NOT GR_SWIG_I_FILE)
    list(APPEND CMAKE_SWIG_FLAGS "-I${CMAKE_CURRENT_SOURCE_DIR}")
endif()

########################################################################
# Setup python install directory
########################################################################

execute_process(
    COMMAND ${Python3_EXECUTABLE} -c "import site; print(site.getsitepackages()[0])"
    OUTPUT_VARIABLE PYTHON3_SITE_PACKAGES
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

########################################################################
# Create swig library
########################################################################

# Set SWIG module name (Python module will be _gr_opus_swig)
set(SWIG_MODULE_gr_opus_swig_NAME _gr_opus_swig)

# Use SWIG_ADD_LIBRARY (uppercase macro - more compatible)
SWIG_ADD_LIBRARY(gr_opus_swig
    TYPE MODULE
    LANGUAGE python
    SOURCES
        gr_opus_swig.i
)

# Set output properties for Python module
set_target_properties(gr_opus_swig PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    OUTPUT_NAME _gr_opus_swig
    PREFIX ""
)

# Build library list, only including GR_SWIG_LIBRARIES if it's not empty
set(GR_OPUS_SWIG_LINK_LIBS
    ${GR_RUNTIME_LIBRARIES}
    ${OPUS_LIBRARIES}
    gnuradio-gr_opus
    ${Python3_LIBRARIES}
)

if(GR_SWIG_LIBRARIES)
    list(APPEND GR_OPUS_SWIG_LINK_LIBS ${GR_SWIG_LIBRARIES})
endif()

SWIG_LINK_LIBRARIES(gr_opus_swig ${GR_OPUS_SWIG_LINK_LIBS})

# Ensure the target is compiled as C++
set_target_properties(gr_opus_swig PROPERTIES
    COMPILE_FLAGS "-x c++"
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
)

########################################################################
# Install swig library and generated Python file
########################################################################

# Install the shared library (Python extension module)
install(TARGETS gr_opus_swig
    LIBRARY DESTINATION ${GR_PYTHON_DIR}/gr_opus
    COMPONENT python
)

# Install the generated Python wrapper file
# SWIG generates this with the name from SWIG_MODULE_<target>_NAME
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/gr_opus_swig.py
    DESTINATION ${GR_PYTHON_DIR}/gr_opus
    COMPONENT python
    OPTIONAL
)
