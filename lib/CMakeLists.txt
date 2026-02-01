########################################################################
# Check for DRED and DNN/FARGAN support in Opus
########################################################################

include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_INCLUDES ${OPUS_INCLUDE_DIRS})
set(CMAKE_REQUIRED_LIBRARIES ${OPUS_LIBRARIES})

check_cxx_source_compiles("
#include <opus/opus.h>
#include <opus/opus_defines.h>
#ifdef OPUS_SET_DRED_DURATION_REQUEST
int main() { return 0; }
#else
#error DRED not available
#endif
" OPUS_HAVE_DRED)

check_cxx_source_compiles("
#include <opus/opus.h>
#include <opus/opus_defines.h>
#ifdef OPUS_SET_DNN_BLOB_REQUEST
int main() { return 0; }
#else
#error DNN_BLOB not available
#endif
" OPUS_HAVE_DNN_BLOB)

if(OPUS_HAVE_DRED)
    message(STATUS "Opus built with DRED support - enabling OPUS_SET_DRED_DURATION")
else()
    message(STATUS "Opus built without DRED support - DRED features disabled")
endif()

if(OPUS_HAVE_DNN_BLOB)
    message(STATUS "Opus DNN/FARGAN blob API available - enabling OPUS_SET_DNN_BLOB")
else()
    message(STATUS "Opus built without DNN blob API - FARGAN external weights disabled")
endif()

########################################################################
# Install library
########################################################################

list(APPEND gr_opus_sources
    opus_encoder_impl.cc
    opus_decoder_impl.cc
)

list(APPEND gr_opus_headers
    opus_encoder_impl.h
    opus_decoder_impl.h
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/../include)

add_library(gnuradio-gr_opus SHARED ${gr_opus_sources} ${gr_opus_headers})

if(OPUS_HAVE_DRED)
    target_compile_definitions(gnuradio-gr_opus PRIVATE OPUS_HAVE_DRED=1)
endif()
if(OPUS_HAVE_DNN_BLOB)
    target_compile_definitions(gnuradio-gr_opus PRIVATE OPUS_HAVE_DNN_BLOB=1)
endif()

target_link_libraries(gnuradio-gr_opus
    ${GR_RUNTIME_LIBRARIES}
    ${OPUS_LIBRARIES}
)

# Ensure all required GNU Radio libraries are linked
# GR_RUNTIME_LIBRARIES should include gnuradio-runtime and gnuradio-pmt
# but we explicitly check to be safe
if(NOT "gnuradio-pmt" IN_LIST GR_RUNTIME_LIBRARIES)
    find_library(GR_PMT_LIBRARY
        NAMES gnuradio-pmt
        PATHS ${GR_LIBRARY_DIR}
             "/usr/local/lib"
             "/usr/lib"
    )
    if(GR_PMT_LIBRARY)
        target_link_libraries(gnuradio-gr_opus ${GR_PMT_LIBRARY})
    endif()
endif()

target_include_directories(gnuradio-gr_opus PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<INSTALL_INTERFACE:include>
    ${CMAKE_CURRENT_BINARY_DIR}/../include
)

set(GR_OPUS_INCLUDE_DIRS $<TARGET_PROPERTY:gnuradio-gr_opus,INTERFACE_INCLUDE_DIRECTORIES>)
set(GR_OPUS_LIBRARIES gnuradio-gr_opus)

install(TARGETS gnuradio-gr_opus
    EXPORT gnuradio-gr_opusTargets
    LIBRARY DESTINATION ${GR_LIBRARY_DIR}
    ARCHIVE DESTINATION ${GR_LIBRARY_DIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT gr_opus_runtime
)

########################################################################
# Install headers
########################################################################

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/gnuradio/gr_opus/api.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/gnuradio/gr_opus/opus_encoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/gnuradio/gr_opus/opus_decoder.h
    DESTINATION ${GR_INCLUDE_DIR}/gnuradio/gr_opus
    COMPONENT gr_opus_devel
)

