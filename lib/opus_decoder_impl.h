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

#ifndef INCLUDED_GR_OPUS_OPUS_DECODER_IMPL_H
#define INCLUDED_GR_OPUS_OPUS_DECODER_IMPL_H

#include <gnuradio/gr_opus/opus_decoder.h>
#include <opus/opus.h>
#include <vector>

namespace gr {
namespace gr_opus {

class opus_decoder_impl : public opus_decoder
{
private:
    OpusDecoder* d_decoder;
    int d_sample_rate;
    int d_channels;
    int d_packet_size;
    int d_frame_size;
    std::vector<unsigned char> d_packet_buffer;
    size_t d_max_buffer_size;

public:
    opus_decoder_impl(int sample_rate, int channels, int packet_size);
    ~opus_decoder_impl();

    int work(int noutput_items,
             gr_vector_const_void_star& input_items,
             gr_vector_void_star& output_items);
};

} // namespace gr_opus
} // namespace gr

#endif /* INCLUDED_GR_OPUS_OPUS_DECODER_IMPL_H */
