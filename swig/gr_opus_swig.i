/* -*- c++ -*- */
/*
 * Copyright 2025 gr-opus author.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

%module(docstring="gr-opus SWIG bindings") _gr_opus_swig

#define GR_OPUS_API

%include "gnuradio.i"

%{
#include "gnuradio/gr_opus/opus_encoder.h"
#include "gnuradio/gr_opus/opus_decoder.h"
%}

// Ignore direct instantiation of abstract classes
%ignore gr::gr_opus::opus_encoder::opus_encoder;
%ignore gr::gr_opus::opus_decoder::opus_decoder;

%include "gnuradio/gr_opus/opus_encoder.h"
%include "gnuradio/gr_opus/opus_decoder.h"
