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
#include "opus_decoder_impl.h"
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <set>

namespace gr {
namespace gr_opus {

opus_decoder::sptr
opus_decoder::make(int sample_rate, int channels, int packet_size)
{
    return gnuradio::get_initial_sptr(new opus_decoder_impl(sample_rate, channels, packet_size));
}

opus_decoder_impl::opus_decoder_impl(int sample_rate, int channels, int packet_size)
    : gr::sync_block("opus_decoder",
                     gr::io_signature::make(1, 1, sizeof(unsigned char)),
                     gr::io_signature::make(1, 1, sizeof(float))),
      d_sample_rate(sample_rate),
      d_channels(channels),
      d_packet_size(packet_size),
      d_frame_size(static_cast<int>(sample_rate * 0.020)),
      d_max_buffer_size(1024 * 1024)
{
    int error;

    d_decoder = opus_decoder_create(sample_rate, channels, &error);
    if (error != OPUS_OK || d_decoder == nullptr) {
        throw std::runtime_error("Failed to create Opus decoder: " + std::string(opus_strerror(error)));
    }
}

opus_decoder_impl::~opus_decoder_impl()
{
    if (d_decoder != nullptr) {
        opus_decoder_destroy(d_decoder);
        d_decoder = nullptr;
    }
}

int opus_decoder_impl::work(int noutput_items,
                            gr_vector_const_void_star& input_items,
                            gr_vector_void_star& output_items)
{
    const unsigned char* in = (const unsigned char*)input_items[0];
    float* out = (float*)output_items[0];

    size_t ninput = noutput_items;

    d_packet_buffer.insert(d_packet_buffer.end(), in, in + ninput);

    if (d_packet_buffer.size() > d_max_buffer_size) {
        size_t excess = d_packet_buffer.size() - d_max_buffer_size;
        d_packet_buffer.erase(d_packet_buffer.begin(), d_packet_buffer.begin() + excess);
    }

    int output_idx = 0;
    std::vector<opus_int16> decoded_pcm(d_frame_size * d_channels);

    if (d_packet_size > 0) {
        while (d_packet_buffer.size() >= static_cast<size_t>(d_packet_size) && output_idx < noutput_items) {
            std::vector<unsigned char> packet(
                d_packet_buffer.begin(),
                d_packet_buffer.begin() + d_packet_size
            );
            d_packet_buffer.erase(d_packet_buffer.begin(), d_packet_buffer.begin() + d_packet_size);

            int decoded_samples = opus_decode(d_decoder,
                                               packet.data(),
                                               d_packet_size,
                                               decoded_pcm.data(),
                                               d_frame_size,
                                               0);

            if (decoded_samples < 0) {
                continue;
            }

            int samples_to_write = decoded_samples * d_channels;
            samples_to_write = std::min(samples_to_write, noutput_items - output_idx);

            for (int i = 0; i < samples_to_write; ++i) {
                float sample = static_cast<float>(decoded_pcm[i]) / 32767.0f;
                sample = std::max(-1.0f, std::min(1.0f, sample));
                out[output_idx + i] = sample;
            }

            output_idx += samples_to_write;
        }
    } else {
        int estimated_packet_size = std::max(40, std::min(400, static_cast<int>(d_packet_buffer.size()) / 5));

        std::set<int> packet_size_candidates;
        if (estimated_packet_size <= static_cast<int>(d_packet_buffer.size())) {
            packet_size_candidates.insert(estimated_packet_size);
        }

        int common_sizes[] = { 60, 80, 100, 120, 150, 180, 200, 250, 300, 350, 400 };
        for (int size : common_sizes) {
            if (size <= static_cast<int>(d_packet_buffer.size())) {
                packet_size_candidates.insert(size);
            }
        }

        int max_candidates = 50;
        for (int size = 1; size <= std::min(4000, static_cast<int>(d_packet_buffer.size())); ++size) {
            if (packet_size_candidates.find(size) == packet_size_candidates.end()) {
                packet_size_candidates.insert(size);
            }
            if (packet_size_candidates.size() >= static_cast<size_t>(max_candidates)) {
                break;
            }
        }

        std::vector<int> candidates_sorted(packet_size_candidates.begin(), packet_size_candidates.end());
        std::sort(candidates_sorted.begin(), candidates_sorted.end());

        while (output_idx < noutput_items && !d_packet_buffer.empty()) {
            bool decoded = false;

            for (int packet_size : candidates_sorted) {
                if (packet_size > static_cast<int>(d_packet_buffer.size())) {
                    continue;
                }

                std::vector<unsigned char> packet(
                    d_packet_buffer.begin(),
                    d_packet_buffer.begin() + packet_size
                );

                int decoded_samples = opus_decode(d_decoder,
                                                   packet.data(),
                                                   packet_size,
                                                   decoded_pcm.data(),
                                                   d_frame_size,
                                                   0);

                if (decoded_samples < 0) {
                    continue;
                }

                bool is_silence = true;
                for (int i = 0; i < decoded_samples * d_channels; ++i) {
                    if (std::abs(decoded_pcm[i]) > 100) {
                        is_silence = false;
                        break;
                    }
                }

                if (!is_silence) {
                    int samples_to_write = decoded_samples * d_channels;
                    samples_to_write = std::min(samples_to_write, noutput_items - output_idx);

                    for (int i = 0; i < samples_to_write; ++i) {
                        float sample = static_cast<float>(decoded_pcm[i]) / 32767.0f;
                        sample = std::max(-1.0f, std::min(1.0f, sample));
                        out[output_idx + i] = sample;
                    }

                    output_idx += samples_to_write;
                    d_packet_buffer.erase(d_packet_buffer.begin(), d_packet_buffer.begin() + packet_size);
                    decoded = true;
                    break;
                }
            }

            if (!decoded) {
                break;
            }
        }
    }

    return output_idx;
}

} // namespace gr_opus
} // namespace gr
