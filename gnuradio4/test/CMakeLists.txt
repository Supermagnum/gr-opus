# SPDX-License-Identifier: GPL-3.0-or-later
function(gr_opus4_add_ut name)
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} PRIVATE gnuradio4::gnuradio-options gnuradio4::gnuradio-core gnuradio4::gnuradio-blocklib-core
                                          gnuradio4::gr-opus ut)
  add_test(NAME ${name} COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:${name}>)
endfunction()

gr_opus4_add_ut(qa_OpusEncoder)
gr_opus4_add_ut(qa_OpusDecoder)
