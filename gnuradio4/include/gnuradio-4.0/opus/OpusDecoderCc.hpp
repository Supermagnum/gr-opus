// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef GNURADIO4_OPUS_OPUSDECODERCC_HPP
#define GNURADIO4_OPUS_OPUSDECODERCC_HPP

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/BlockRegistry.hpp>
#include <gnuradio-4.0/Message.hpp>
#include <gnuradio-4.0/Port.hpp>
#include <gnuradio-4.0/Tensor.hpp>
#include <gnuradio-4.0/annotated.hpp>
#include <gnuradio-4.0/opus/detail/OpusHelpers.hpp>

#include <opus/opus.h>

#include <algorithm>
#include <cstddef>
#include <deque>
#include <iostream>
#include <span>
#include <vector>

namespace gnuradio4::opus {

GR_REGISTER_BLOCK(gnuradio4::opus::OpusDecoderCc)

/** Opus PDU to float PCM (legacy gr-opus decoder). Avoids collision with libopus OpusDecoder. */
struct OpusDecoderCc : gr::Block<OpusDecoderCc, gr::NoTagPropagation> {
    using Description =
        gr::Doc<"Decode pdu_bytes PDUs into float PCM; each processBulk outputs one decoded frame at out port.">;

    gr::MsgPortIn       pdu_in{};
    gr::PortOut<float> out{};

    gr::Annotated<int, "sample_rate", gr::Doc<"Output PCM sample rate (8000 ... 48000 Hz)">>        sample_rate = 48000;
    gr::Annotated<int, "channels", gr::Doc<"1 mono or 2 stereo">>                                    channels    = 1;
    gr::Annotated<int, "packet_size", gr::Doc<"PDU size bytes when fixed non-zero else tensor">> packet_size = 0;
    gr::Annotated<float, "frame_size_ms", gr::Doc<"Frame duration ms matching encoder">>            frame_size_ms = 20.0F;

    GR_MAKE_REFLECTABLE(OpusDecoderCc, pdu_in, out, sample_rate, channels, packet_size, frame_size_ms);

    ::OpusDecoder*                        _opusDecoder           = nullptr;
    int                                   _samplesPerChannel     = 0;
    std::size_t                           _interleavedOutSamples = 0;
    std::deque<std::vector<std::uint8_t>> _packetQueue{};
    static constexpr std::size_t          kQueueCap = 512UZ;

    void destroyDecoder() noexcept {
        if (_opusDecoder != nullptr) {
            ::opus_decoder_destroy(_opusDecoder);
            _opusDecoder = nullptr;
        }
    }

    [[nodiscard]] bool staticSettingsOk() const noexcept {
        return detail::isAllowedOpusSampleRate(sample_rate.value) && (channels.value == 1 || channels.value == 2)
            && detail::isAllowedFrameMs(frame_size_ms.value) && packet_size.value >= 0;
    }

    [[nodiscard]] bool recreateDecoderFromSettings() noexcept {
        destroyDecoder();
        int err      = OPUS_OK;
        _opusDecoder = ::opus_decoder_create(static_cast<opus_int32>(sample_rate.value), channels.value, &err);
        if (err != OPUS_OK || _opusDecoder == nullptr) [[unlikely]] {
            std::cerr << "gr-opus4: opus_decoder_create failed: " << opus_strerror(err) << "\n";
            return false;
        }
        return true;
    }

    void refreshFrameSizing() noexcept {
        _samplesPerChannel     = detail::frameSamplesPerChannel(sample_rate.value, frame_size_ms.value);
        _interleavedOutSamples = detail::interleavedSampleCount(_samplesPerChannel, channels.value);
    }

    static void writeSilence(std::span<float> destination) noexcept { std::fill(destination.begin(), destination.end(), 0.0F); }

    void start() noexcept {
        if (!staticSettingsOk()) [[unlikely]] {
            std::cerr << "gr-opus4: OpusDecoderCc invalid configuration.\n";
            this->requestStop();
            return;
        }
        refreshFrameSizing();
        if (!recreateDecoderFromSettings()) [[unlikely]] {
            this->requestStop();
        }
    }

    void stop() noexcept {
        destroyDecoder();
        _packetQueue.clear();
    }

    void settingsChanged(const gr::property_map&, const gr::property_map& newSettings) noexcept {
        const bool touches =
            newSettings.contains("sample_rate") || newSettings.contains("channels") || newSettings.contains("packet_size")
            || newSettings.contains("frame_size_ms");
        if (!touches) [[likely]] {
            return;
        }
        if (!staticSettingsOk()) [[unlikely]] {
            destroyDecoder();
            this->requestStop();
            return;
        }
        refreshFrameSizing();
        if (!recreateDecoderFromSettings()) [[unlikely]] {
            this->requestStop();
        }
    }

    void processMessages(gr::MsgPortIn& port, std::span<const gr::Message> messages) noexcept {
        if (std::addressof(port) != std::addressof(pdu_in)) [[unlikely]] {
            return;
        }
        for (const gr::Message& message : messages) {
            if (!message.data.has_value()) [[unlikely]] {
                continue;
            }
            const gr::property_map&         body = message.data.value();
            const gr::Tensor<std::uint8_t>* pdu  = detail::pduTensorFromMap(body);
            if (pdu == nullptr) [[unlikely]] {
                continue;
            }
            std::vector<std::uint8_t> pkt(pdu->begin(), pdu->end());
            if (packet_size.value > 0) {
                const auto expected = static_cast<std::size_t>(packet_size.value);
                if (pkt.size() != expected) [[unlikely]] {
                    std::cerr << "gr-opus4: pdu_bytes length mismatch (expected " << packet_size.value << " bytes).\n";
                    continue;
                }
            }
            if (_packetQueue.size() >= kQueueCap) [[unlikely]] {
                _packetQueue.pop_front();
            }
            _packetQueue.emplace_back(std::move(pkt));
        }
    }

    [[nodiscard]] gr::work::Status processBulk(std::span<float> output) noexcept {
        if (_opusDecoder == nullptr) [[unlikely]] {
            return gr::work::Status::ERROR;
        }
        if (output.size() != _interleavedOutSamples) [[unlikely]] {
            return gr::work::Status::ERROR;
        }
        if (_packetQueue.empty()) {
            return gr::work::Status::INSUFFICIENT_INPUT_ITEMS;
        }
        std::vector<std::uint8_t> packet = std::move(_packetQueue.front());
        _packetQueue.pop_front();
        const opus_int32 len = static_cast<opus_int32>(packet.size());
        if (len <= 0) [[unlikely]] {
            writeSilence(output);
            return gr::work::Status::OK;
        }
        std::vector<float> pcm(_interleavedOutSamples);
        const int          decodedSamples =
            opus_decode_float(_opusDecoder, packet.data(), len, pcm.data(), _samplesPerChannel, 0);
        if (decodedSamples < 0) [[unlikely]] {
            std::cerr << "gr-opus4: opus_decode_float failed: " << opus_strerror(decodedSamples) << "\n";
            writeSilence(output);
            return gr::work::Status::OK;
        }
        const int totalOut = decodedSamples * channels.value;
        for (std::size_t idx = 0; idx < output.size(); ++idx) {
            if (idx >= static_cast<std::size_t>(totalOut)) {
                output[idx] = 0.0F;
                continue;
            }
            float s     = pcm[idx];
            output[idx] = std::max(-1.0F, std::min(1.0F, s));
        }
        return gr::work::Status::OK;
    }
};

} // namespace gnuradio4::opus

#endif
