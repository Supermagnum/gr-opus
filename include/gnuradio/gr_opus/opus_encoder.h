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

#ifndef INCLUDED_GR_OPUS_OPUS_ENCODER_H
#define INCLUDED_GR_OPUS_OPUS_ENCODER_H

#include <gnuradio/sync_block.h>
#include <gnuradio/gr_opus/api.h>

namespace gr {
namespace gr_opus {

class GR_OPUS_API opus_encoder : virtual public gr::sync_block
{
public:
    typedef std::shared_ptr<opus_encoder> sptr;

    static sptr make(int sample_rate, int channels, int bitrate, const std::string& application, bool enable_fargan_voice = false, const std::string& dnn_blob_path = "");
};

} // namespace gr_opus
} // namespace gr

#endif /* INCLUDED_GR_OPUS_OPUS_ENCODER_H */
