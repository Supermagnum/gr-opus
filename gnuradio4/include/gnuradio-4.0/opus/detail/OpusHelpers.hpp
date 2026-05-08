// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef GNURADIO4_OPUS_DETAIL_HELPERS_HPP
#define GNURADIO4_OPUS_DETAIL_HELPERS_HPP

#include <gnuradio-4.0/Tag.hpp>
#include <gnuradio-4.0/Tensor.hpp>

#include <opus/opus.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace gnuradio4::opus::detail {

[[nodiscard]] constexpr bool isAllowedOpusSampleRate(int sampleRateHz) noexcept {
    switch (sampleRateHz) {
    case 8000:
    case 12000:
    case 16000:
    case 24000:
    case 48000:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] inline bool isAllowedFrameMs(float frameMs) noexcept {
    constexpr float kEps = 1e-3f;
    const float     v    = std::fabs(frameMs);
    return (std::fabs(v - 2.5f) < kEps) || (std::fabs(v - 5.0f) < kEps) || (std::fabs(v - 10.0f) < kEps) || (std::fabs(v - 20.0f) < kEps)
        || (std::fabs(v - 40.0f) < kEps) || (std::fabs(v - 60.0f) < kEps);
}

/** Samples per channel for one Opus frame (opus encode/decode frame length). */
[[nodiscard]] inline int frameSamplesPerChannel(int sampleRateHz, float frameMs) noexcept {
    const double x = static_cast<double>(sampleRateHz) * static_cast<double>(frameMs) / 1000.0;
    return static_cast<int>(std::lround(x));
}

[[nodiscard]] inline std::size_t interleavedSampleCount(int samplesPerChannel, int channels) noexcept {
    return static_cast<std::size_t>(std::max(0, samplesPerChannel)) * static_cast<std::size_t>(std::max(1, channels));
}

[[nodiscard]] inline std::int32_t applicationKind(std::string_view application) noexcept {
    if (application == "voip") {
        return OPUS_APPLICATION_VOIP;
    }
    if (application == "lowdelay") {
        return OPUS_APPLICATION_RESTRICTED_LOWDELAY;
    }
    return OPUS_APPLICATION_AUDIO;
}

[[nodiscard]] inline const gr::Tensor<std::uint8_t>* pduTensorFromMap(const gr::property_map& map) noexcept {
    const std::pmr::string key = gr::convert_string_domain(std::string_view("pdu_bytes"));
    const auto             it  = map.find(key);
    if (it == map.end()) {
        return nullptr;
    }
    return it->second.get_if<gr::Tensor<std::uint8_t>>();
}

} // namespace gnuradio4::opus::detail

#endif
