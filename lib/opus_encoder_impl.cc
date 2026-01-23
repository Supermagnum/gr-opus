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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include "opus_encoder_impl.h"
#include <string>
#include <stdexcept>
#include <algorithm>
#include <cstring>

namespace gr {
namespace gr_opus {

opus_encoder::sptr
opus_encoder::make(int sample_rate, int channels, int bitrate, const std::string& application)
{
    return gnuradio::get_initial_sptr(new opus_encoder_impl(sample_rate, channels, bitrate, application));
}

int opus_encoder_impl::application_string_to_int(const std::string& application)
{
    if (application == "voip") {
        return OPUS_APPLICATION_VOIP;
    } else if (application == "lowdelay") {
        return OPUS_APPLICATION_RESTRICTED_LOWDELAY;
    } else {
        return OPUS_APPLICATION_AUDIO;
    }
}

opus_encoder_impl::opus_encoder_impl(int sample_rate, int channels, int bitrate, const std::string& application)
    : gr::sync_block("opus_encoder",
                     gr::io_signature::make(1, 1, sizeof(float)),
                     gr::io_signature::make(1, 1, sizeof(unsigned char))),
      d_sample_rate(sample_rate),
      d_channels(channels),
      d_bitrate(bitrate),
      d_frame_size(static_cast<int>(sample_rate * 0.020)),
      d_max_buffer_samples(sample_rate * channels * 10)
{
    int error;
    int application_int = application_string_to_int(application);

    d_encoder = opus_encoder_create(sample_rate, channels, application_int, &error);
    if (error != OPUS_OK || d_encoder == nullptr) {
        throw std::runtime_error("Failed to create Opus encoder: " + std::string(opus_strerror(error)));
    }

    error = opus_encoder_ctl(d_encoder, OPUS_SET_BITRATE(bitrate));
    if (error != OPUS_OK) {
        opus_encoder_destroy(d_encoder);
        throw std::runtime_error("Failed to set Opus encoder bitrate: " + std::string(opus_strerror(error)));
    }
}

opus_encoder_impl::~opus_encoder_impl()
{
    if (d_encoder != nullptr) {
        opus_encoder_destroy(d_encoder);
        d_encoder = nullptr;
    }
}

int opus_encoder_impl::work(int noutput_items,
                            gr_vector_const_void_star& input_items,
                            gr_vector_void_star& output_items)
{
    const float* in = (const float*)input_items[0];
    unsigned char* out = (unsigned char*)output_items[0];

    size_t ninput = noutput_items;

    d_sample_buffer.insert(d_sample_buffer.end(), in, in + ninput);

    if (d_sample_buffer.size() > d_max_buffer_samples) {
        size_t excess = d_sample_buffer.size() - d_max_buffer_samples;
        d_sample_buffer.erase(d_sample_buffer.begin(), d_sample_buffer.begin() + excess);
    }

    int output_idx = 0;
    int frame_size_samples = d_frame_size * d_channels;
    int frames_encoded = 0;

    std::vector<opus_int16> int16_frame(frame_size_samples);

    while (d_sample_buffer.size() >= static_cast<size_t>(frame_size_samples) && output_idx < noutput_items) {
        std::vector<float> frame_samples(
            d_sample_buffer.begin(),
            d_sample_buffer.begin() + frame_size_samples
        );
        d_sample_buffer.erase(d_sample_buffer.begin(), d_sample_buffer.begin() + frame_size_samples);

        for (size_t i = 0; i < frame_samples.size(); ++i) {
            float sample = frame_samples[i];
            sample = std::max(-1.0f, std::min(1.0f, sample));
            int16_frame[i] = static_cast<opus_int16>(sample * 32767.0f);
        }

        unsigned char encoded_data[4000];
        int encoded_len = opus_encode(d_encoder,
                                      int16_frame.data(),
                                      d_frame_size,
                                      encoded_data,
                                      4000);

        if (encoded_len < 0) {
            break;
        }

        if (output_idx + encoded_len <= noutput_items) {
            std::memcpy(out + output_idx, encoded_data, encoded_len);
            output_idx += encoded_len;
            frames_encoded++;
        } else {
            d_sample_buffer.insert(d_sample_buffer.begin(), frame_samples.begin(), frame_samples.end());
            break;
        }
    }

    return output_idx;
}

} // namespace gr_opus
} // namespace gr
