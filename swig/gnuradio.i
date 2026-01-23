/* -*- c++ -*- */
/*
 * Minimal gnuradio.i for gr-opus
 * This file provides the necessary SWIG interface definitions
 * when gnuradio.i is not installed with GNU Radio
 */

%include <std_string.i>

%{
#include <memory>
#include <gnuradio/attributes.h>
%}

// Forward declarations for GNU Radio base classes
namespace gr {
    class basic_block;
    class block;
    class sync_block;
}

%define GR_SWIG_BLOCK_MAGIC2(PKG, BASE_NAME)
%enddef
