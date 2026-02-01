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
#include <fstream>
#include <vector>

namespace gr {
namespace gr_opus {

opus_decoder::sptr
opus_decoder::make(int sample_rate, int channels, int packet_size, const std::string& dnn_blob_path)
{
    return gnuradio::get_initial_sptr(new opus_decoder_impl(sample_rate, channels, packet_size, dnn_blob_path));
}

opus_decoder_impl::opus_decoder_impl(int sample_rate, int channels, int packet_size, const std::string& dnn_blob_path)
    : gr::sync_block("opus_decoder",
                     gr::io_signature::make(1, 1, sizeof(unsigned char)),
                     gr::io_signature::make(1, 1, sizeof(float))),
      d_sample_rate(sample_rate),
      d_channels(channels),
      d_packet_size(packet_size),
      d_frame_size(static_cast<int>(sample_rate * 0.020)),
      d_max_buffer_size(1024 * 1024)
#ifdef OPUS_HAVE_DRED
      , d_dred_decoder(nullptr)
      , d_dred(nullptr)
      , d_lost_count(0)
#endif
{
    int error;

    d_decoder = opus_decoder_create(sample_rate, channels, &error);
    if (error != OPUS_OK || d_decoder == nullptr) {
        throw std::runtime_error("Failed to create Opus decoder: " + std::string(opus_strerror(error)));
    }

#ifdef OPUS_HAVE_DNN_BLOB
    std::vector<char> blob;
    if (!dnn_blob_path.empty()) {
        std::ifstream f(dnn_blob_path, std::ios::binary | std::ios::ate);
        if (!f) {
            opus_decoder_destroy(d_decoder);
            throw std::runtime_error("Failed to open DNN blob file: " + dnn_blob_path);
        }
        blob.resize(f.tellg());
        f.seekg(0);
        if (!f.read(blob.data(), blob.size())) {
            opus_decoder_destroy(d_decoder);
            throw std::runtime_error("Failed to read DNN blob file: " + dnn_blob_path);
        }
        error = opus_decoder_ctl(d_decoder, OPUS_SET_DNN_BLOB(blob.data(), static_cast<int>(blob.size())));
        if (error != OPUS_OK) {
            opus_decoder_destroy(d_decoder);
            throw std::runtime_error("Failed to set Opus DNN blob (FARGAN): " + std::string(opus_strerror(error)));
        }
    }
#endif

#ifdef OPUS_HAVE_DRED
    d_dred_decoder = opus_dred_decoder_create(&error);
    if (error != OPUS_OK || d_dred_decoder == nullptr) {
        opus_decoder_destroy(d_decoder);
        throw std::runtime_error("Failed to create Opus DRED decoder: " + std::string(opus_strerror(error)));
    }
    d_dred = opus_dred_alloc(&error);
    if (error != OPUS_OK || d_dred == nullptr) {
        opus_dred_decoder_destroy(d_dred_decoder);
        opus_decoder_destroy(d_decoder);
        throw std::runtime_error("Failed to alloc Opus DRED state: " + std::string(opus_strerror(error)));
    }
#ifdef OPUS_HAVE_DNN_BLOB
    if (!dnn_blob_path.empty() && !blob.empty()) {
        error = opus_dred_decoder_ctl(d_dred_decoder, OPUS_SET_DNN_BLOB(blob.data(), static_cast<int>(blob.size())));
        if (error != OPUS_OK) {
            opus_dred_free(d_dred);
            opus_dred_decoder_destroy(d_dred_decoder);
            opus_decoder_destroy(d_decoder);
            throw std::runtime_error("Failed to set DRED DNN blob: " + std::string(opus_strerror(error)));
        }
    }
#endif
#endif
}

opus_decoder_impl::~opus_decoder_impl()
{
#ifdef OPUS_HAVE_DRED
    if (d_dred != nullptr) {
        opus_dred_free(d_dred);
        d_dred = nullptr;
    }
    if (d_dred_decoder != nullptr) {
        opus_dred_decoder_destroy(d_dred_decoder);
        d_dred_decoder = nullptr;
    }
#endif
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
    std::vector<float> dred_pcm(d_frame_size * d_channels);

    if (d_packet_size > 0) {
        while (d_packet_buffer.size() >= static_cast<size_t>(d_packet_size) && output_idx < noutput_items) {
            std::vector<unsigned char> packet(
                d_packet_buffer.begin(),
                d_packet_buffer.begin() + d_packet_size
            );
            d_packet_buffer.erase(d_packet_buffer.begin(), d_packet_buffer.begin() + d_packet_size);

#ifdef OPUS_HAVE_DRED
            if (d_lost_count > 0) {
                int dred_end = 0;
                int dred_amount = opus_dred_parse(d_dred_decoder, d_dred, packet.data(), d_packet_size,
                    d_lost_count * d_frame_size, d_sample_rate, &dred_end, 0);
                if (dred_amount > 0) {
                    for (int fr = 0; fr < d_lost_count && output_idx < noutput_items; ++fr) {
                        int dred_offset = (d_lost_count - fr) * d_frame_size;
                        int samples = opus_decoder_dred_decode_float(d_decoder, d_dred, dred_offset,
                            dred_pcm.data(), d_frame_size);
                        if (samples > 0) {
                            int to_write = std::min(samples * d_channels, noutput_items - output_idx);
                            for (int i = 0; i < to_write; ++i) {
                                out[output_idx + i] = std::max(-1.0f, std::min(1.0f, dred_pcm[i]));
                            }
                            output_idx += to_write;
                        }
                    }
                }
                d_lost_count = 0;
            }
#endif

            int decoded_samples = opus_decode(d_decoder,
                                               packet.data(),
                                               d_packet_size,
                                               decoded_pcm.data(),
                                               d_frame_size,
                                               0);

            if (decoded_samples < 0) {
#ifdef OPUS_HAVE_DRED
                d_lost_count++;
#endif
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
