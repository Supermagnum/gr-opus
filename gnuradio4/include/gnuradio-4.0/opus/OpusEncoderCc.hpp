// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef GNURADIO4_OPUS_OPUSENCODERCC_HPP
#define GNURADIO4_OPUS_OPUSENCODERCC_HPP

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/BlockRegistry.hpp>
#include <gnuradio-4.0/Message.hpp>
#include <gnuradio-4.0/Port.hpp>
#include <gnuradio-4.0/Tensor.hpp>
#include <gnuradio-4.0/annotated.hpp>
#include <gnuradio-4.0/opus/detail/OpusHelpers.hpp>

#include <opus/opus.h>

#include <array>
#include <cstddef>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifdef HAVE_OPUS_DRED
#include <opus/opus_defines.h>
#endif

namespace gnuradio4::opus {

GR_REGISTER_BLOCK(gnuradio4::opus::OpusEncoderCc)

/** Float PCM to Opus packets (legacy gr-opus encoder). libopus type is also named OpusEncoder; this struct is OpusEncoderCc. */
struct OpusEncoderCc : gr::Block<OpusEncoderCc, gr::NoTagPropagation> {
    using Description = gr::Doc<"Encode float32 PCM to Opus; one pdu_bytes Tensor PDU per consumed frame via pdu_out.">;

    gr::PortIn<float> in{};
    gr::MsgPortOut    pdu_out{};

    gr::Annotated<int, "sample_rate", gr::Doc<"PCM sample rate (8000 / 12000 / 16000 / 24000 / 48000 Hz)">>     sample_rate   = 48000;
    gr::Annotated<int, "channels", gr::Doc<"1 mono or 2 stereo">>                                                   channels      = 1;
    gr::Annotated<int, "bitrate", gr::Doc<"Target bitrate bits per second">>                                        bitrate       = 64000;
    gr::Annotated<std::string, "application", gr::Doc<"voip, audio, or lowdelay">>                                 application   = std::string("audio");
    gr::Annotated<float, "frame_size_ms", gr::Doc<"Frame duration ms: 2.5, 5, 10, 20, 40, or 60">>                  frame_size_ms = 20.0F;
    gr::Annotated<bool, "enable_fargan", gr::Doc<"Enable DRED when Opus supports OPUS_SET_DRED_DURATION">>        enable_fargan = false;

    GR_MAKE_REFLECTABLE(OpusEncoderCc, in, pdu_out, sample_rate, channels, bitrate, application, frame_size_ms, enable_fargan);

    ::OpusEncoder*        _opusEncoder          = nullptr;
    int                   _samplesPerChannel    = 0;
    std::size_t           _interleavedSamples   = 0;
    bool                  _loggedFarganIgnored = false;

    void publishPduTensor(std::vector<std::uint8_t>&& bytes) noexcept {
        gr::property_map body;
        body[gr::convert_string_domain(std::string_view("pdu_bytes"))] = gr::pmt::Value(gr::Tensor<std::uint8_t>(std::move(bytes)));
        gr::Message msg;
        msg.cmd  = gr::message::Command::Notify;
        msg.data = std::move(body);
        gr::WriterSpanLike auto w = pdu_out.streamWriter().template reserve<gr::SpanReleasePolicy::ProcessAll>(1UZ);
        w[0]                      = std::move(msg);
        w.publish(1UZ);
    }

    void destroyEncoder() noexcept {
        if (_opusEncoder != nullptr) {
            ::opus_encoder_destroy(_opusEncoder);
            _opusEncoder = nullptr;
        }
    }

    [[nodiscard]] bool staticSettingsOk() const noexcept {
        return detail::isAllowedOpusSampleRate(sample_rate.value) && (channels.value == 1 || channels.value == 2)
            && detail::isAllowedFrameMs(frame_size_ms.value) && (bitrate.value > 0);
    }

    [[nodiscard]] bool recreateEncoderFromSettings() noexcept {
        destroyEncoder();
        int           err     = OPUS_OK;
        const int     appKind = detail::applicationKind(std::string_view(application.value));
        _opusEncoder          = ::opus_encoder_create(static_cast<opus_int32>(sample_rate.value), channels.value, appKind, &err);
        if (err != OPUS_OK || _opusEncoder == nullptr) {
            std::cerr << "gr-opus4: OpusEncoderCc opus_encoder_create failed: " << opus_strerror(err) << "\n";
            return false;
        }
        if (::opus_encoder_ctl(_opusEncoder, OPUS_SET_BITRATE(bitrate.value)) != OPUS_OK) {
            std::cerr << "gr-opus4: OpusEncoderCc OPUS_SET_BITRATE failed.\n";
            destroyEncoder();
            return false;
        }
#ifdef HAVE_OPUS_DRED
        if (enable_fargan.value) {
            const int dredDuration = 5;
            err                    = ::opus_encoder_ctl(_opusEncoder, OPUS_SET_DRED_DURATION(dredDuration));
            if (err != OPUS_OK) {
                std::cerr << "gr-opus4: OpusEncoderCc OPUS_SET_DRED_DURATION failed: " << opus_strerror(err) << "\n";
                destroyEncoder();
                return false;
            }
        }
#else
        if (enable_fargan.value && !_loggedFarganIgnored) {
            std::cerr << "gr-opus4: enable_fargan requested but Opus lacks OPUS_SET_DRED_DURATION_REQUEST; ignoring.\n";
            _loggedFarganIgnored = true;
        }
#endif
        return true;
    }

    void refreshFrameSizing() noexcept {
        _samplesPerChannel  = detail::frameSamplesPerChannel(sample_rate.value, frame_size_ms.value);
        _interleavedSamples = detail::interleavedSampleCount(_samplesPerChannel, channels.value);
    }

    void start() noexcept {
        if (!staticSettingsOk()) {
            std::cerr << "gr-opus4: OpusEncoderCc invalid configuration (sample_rate / channels / frame_size_ms / bitrate).\n";
            this->requestStop();
            return;
        }
        refreshFrameSizing();
        if (!recreateEncoderFromSettings()) [[unlikely]] {
            this->requestStop();
        }
    }

    void stop() noexcept { destroyEncoder(); }

    void settingsChanged(const gr::property_map&, const gr::property_map& newSettings) noexcept {
        const bool touches = newSettings.contains("sample_rate") || newSettings.contains("channels") || newSettings.contains("bitrate")
            || newSettings.contains("application") || newSettings.contains("frame_size_ms") || newSettings.contains("enable_fargan");
        if (!touches) [[likely]] {
            return;
        }
        if (!staticSettingsOk()) [[unlikely]] {
            destroyEncoder();
            this->requestStop();
            return;
        }
        refreshFrameSizing();
        if (!recreateEncoderFromSettings()) [[unlikely]] {
            this->requestStop();
        }
    }

    [[nodiscard]] gr::work::Status processBulk(std::span<const float> inputSpan) noexcept {
        if (_opusEncoder == nullptr) [[unlikely]] {
            return gr::work::Status::ERROR;
        }
        if (inputSpan.size() < _interleavedSamples) {
            return gr::work::Status::INSUFFICIENT_INPUT_ITEMS;
        }
        std::vector<opus_int16> pcm(_interleavedSamples);
        for (std::size_t i = 0; i < _interleavedSamples; ++i) {
            const float s = std::max(-1.0F, std::min(1.0F, inputSpan[static_cast<std::ptrdiff_t>(i)]));
            pcm[i]        = static_cast<opus_int16>(s * 32767.0F);
        }
        constexpr std::size_t                         kEncodeMax = 4000UZ;
        std::array<unsigned char, kEncodeMax>         packet{};
        const int encodedLength =
            opus_encode(_opusEncoder, pcm.data(), _samplesPerChannel, packet.data(), static_cast<opus_int32>(kEncodeMax));
        if (encodedLength < 0) [[unlikely]] {
            std::cerr << "gr-opus4: opus_encode failed: " << opus_strerror(encodedLength) << "\n";
            publishPduTensor(std::vector<std::uint8_t>{});
            return gr::work::Status::OK;
        }
        std::vector<std::uint8_t> outBytes(static_cast<std::size_t>(encodedLength));
        std::copy(packet.begin(), packet.begin() + encodedLength, outBytes.begin());
        publishPduTensor(std::move(outBytes));
        return gr::work::Status::OK;
    }
};

} // namespace gnuradio4::opus

#endif
