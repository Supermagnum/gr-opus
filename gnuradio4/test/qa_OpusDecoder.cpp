// SPDX-License-Identifier: GPL-3.0-or-later
#include <boost/ut.hpp>

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/Message.hpp>
#include <gnuradio-4.0/Port.hpp>
#include <gnuradio-4.0/Sequence.hpp>
#include <gnuradio-4.0/Tag.hpp>
#include <gnuradio-4.0/Tensor.hpp>
#include <gnuradio-4.0/Value.hpp>
#include <gnuradio-4.0/opus/OpusDecoderCc.hpp>
#include <gnuradio-4.0/opus/OpusEncoderCc.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <random>
#include <string>
#include <string_view>
#include <vector>

using namespace boost::ut;

namespace {

[[nodiscard]] constexpr int stN(gr::work::Status st) noexcept {
    return static_cast<int>(st);
}

void subscribeEncoderPdu(gnuradio4::opus::OpusEncoderCc& enc, gr::MsgPortIn& sink) {
    expect(enc.pdu_out.connect(sink).has_value());
}

void pushPduMessage(gr::MsgPortOut& downstream, std::vector<std::uint8_t>&& bytes) {
    gr::property_map               body;
    body[gr::convert_string_domain(std::string_view("pdu_bytes"))] =
        gr::pmt::Value(gr::Tensor<std::uint8_t>(std::move(bytes)));
    gr::Message message;
    message.cmd  = gr::message::Command::Notify;
    message.data = std::move(body);
    gr::WriterSpanLike auto w = downstream.streamWriter().template reserve<gr::SpanReleasePolicy::ProcessAll>(1UZ);
    w[0]                      = std::move(message);
    w.publish(1UZ);
}

[[nodiscard]] const gr::Tensor<std::uint8_t>* pduTensorFromSink(gr::MsgPortIn& sink) {
    expect(eq(sink.streamReader().available(), 1UZ));
    auto                 readerSpan = sink.streamReader().template get<gr::SpanReleasePolicy::ProcessAll>(1UZ);
    const gr::Message& message       = readerSpan[0UZ];
    expect(message.data.has_value());
    const gr::property_map& body = message.data.value();
    const auto              key =
        gr::convert_string_domain(std::string_view("pdu_bytes"));
    const auto              it = body.find(key);
    expect(it != body.end());
    return it->second.get_if<gr::Tensor<std::uint8_t>>();
}

[[nodiscard]] std::vector<std::uint8_t> pduBytesFromEncoderFrame(gnuradio4::opus::OpusEncoderCc& enc, gr::MsgPortIn& sink,
    const float* pcm, std::size_t sampleCount) {
    subscribeEncoderPdu(enc, sink);
    expect(eq(stN(enc.processBulk(std::span<const float>(pcm, sampleCount))), stN(gr::work::Status::OK)));
    const gr::Tensor<std::uint8_t>* t = pduTensorFromSink(sink);
    expect(t != nullptr);
    return std::vector<std::uint8_t>(t->begin(), t->end());
}

[[nodiscard]] double toneEnergyFraction(std::span<const float> x, double freqHz, double sampleRateHz) {
    const double                                  omega =
        static_cast<double>(2.0L * std::numbers::pi) * freqHz / sampleRateHz;
    double crs = 0.0;
    double cis = 0.0;
    double pwr = 0.0;
    for (std::size_t n = 0; n < x.size(); ++n) {
        const double v =
            static_cast<double>(x[static_cast<std::ptrdiff_t>(n)]);
        const double tn = omega * static_cast<double>(static_cast<long>(n));
        crs += v * std::cos(tn);
        cis += v * std::sin(tn);
        pwr += v * v;
    }
    const double tone = crs * crs + cis * cis;
    const double denom = std::max(1e-30, pwr * static_cast<double>(x.size()));
    return tone / denom;
}

} // namespace

const suite<"OpusDecoder"> OpusDecoderSuite = [] {
    "valid_rates_initialise"_test = [] {
        constexpr int srates[] = { 8000, 12000, 16000, 24000, 48000 };
        for (int sr : srates) {
            gnuradio4::opus::OpusDecoderCc dec(
                gr::property_map{ { "name", std::string("d") }, { "sample_rate", sr }, { "channels", 1 }, { "packet_size", 0 },
                    { "frame_size_ms", 20.F } });
            dec.init(std::make_shared<gr::Sequence>());
            dec.start();
            const int sp =
                gnuradio4::opus::detail::frameSamplesPerChannel(sr, dec.frame_size_ms.value);
            std::vector<float>     out(static_cast<std::size_t>(sp * dec.channels.value));
            std::vector<std::uint8_t> bogus(96U);
            gr::MsgPortOut          src;
            expect(src.connect(dec.pdu_in).has_value());
            pushPduMessage(src, std::move(bogus));
            dec.processScheduledMessages();
            expect(eq(stN(dec.processBulk(std::span<float>(out.data(), out.size()))), stN(gr::work::Status::OK)));
            dec.stop();
        }
    };

    "produce_expected_output_samples_per_packet"_test = [] {
        gnuradio4::opus::OpusDecoderCc dec(
            gr::property_map{ { "name", std::string("d2") }, { "sample_rate", 48000 }, { "channels", 2 }, { "packet_size", 0 },
                { "frame_size_ms", 10.F } });
        dec.init(std::make_shared<gr::Sequence>());
        dec.start();
        gnuradio4::opus::OpusEncoderCc enc(
            gr::property_map{ { "name", std::string("e2") }, { "sample_rate", dec.sample_rate.value }, { "channels", dec.channels.value },
                { "bitrate", 128000 }, { "application", std::string("audio") }, { "frame_size_ms", dec.frame_size_ms.value } });
        enc.init(std::make_shared<gr::Sequence>());
        enc.start();
        const int                                                  sp =
            gnuradio4::opus::detail::frameSamplesPerChannel(dec.sample_rate.value, dec.frame_size_ms.value);
        const std::size_t                                           need =
            gnuradio4::opus::detail::interleavedSampleCount(sp, dec.channels.value);
        const std::vector<float> pcm(need, 0.01F);
        gr::MsgPortIn                                                 encPduSink;
        const std::vector<std::uint8_t> pdu =
            pduBytesFromEncoderFrame(enc, encPduSink, pcm.data(), pcm.size());
        std::vector<float> out(need);
        gr::MsgPortOut           toDecoder;
        expect(toDecoder.connect(dec.pdu_in).has_value());
        pushPduMessage(toDecoder, std::vector<std::uint8_t>(pdu));
        dec.processScheduledMessages();
        expect(eq(stN(dec.processBulk(std::span<float>(out.data(), need))), stN(gr::work::Status::OK)));

        expect(eq(out.size(), need));
        dec.stop();
        enc.stop();
    };

    "silence_on_corrupt_or_empty"_test = [] {
        gnuradio4::opus::OpusDecoderCc dec(
            gr::property_map{ { "name", std::string("d3") }, { "sample_rate", 16000 }, { "channels", 1 }, { "packet_size", 0 },
                { "frame_size_ms", 20.F } });
        dec.init(std::make_shared<gr::Sequence>());
        dec.start();
        const int sp =
            gnuradio4::opus::detail::frameSamplesPerChannel(dec.sample_rate.value, dec.frame_size_ms.value);
        std::vector<float>         out(static_cast<std::size_t>(sp));
        gr::MsgPortOut             src;
        expect(src.connect(dec.pdu_in).has_value());

        pushPduMessage(src, std::vector<std::uint8_t>{});
        dec.processScheduledMessages();
        expect(eq(stN(dec.processBulk(std::span<float>(out.data(), out.size()))), stN(gr::work::Status::OK)));
        expect(eq(std::abs(out.front()) < 1e-6F, true));

        std::mt19937                            rng(42U);
        std::uniform_int_distribution<unsigned> dist(0U, 255U);
        std::vector<std::uint8_t>               garbage(80U);
        for (auto& b : garbage) {
            b = static_cast<std::uint8_t>(dist(rng));
        }
        pushPduMessage(src, std::move(garbage));
        dec.processScheduledMessages();
        expect(eq(stN(dec.processBulk(std::span<float>(out.data(), out.size()))), stN(gr::work::Status::OK)));

        dec.stop();
    };

    "round_trip_silence_finite_range"_test = [] {
        gnuradio4::opus::OpusDecoderCc dec(
            gr::property_map{ { "name", std::string("d4") }, { "sample_rate", 48000 }, { "channels", 1 }, { "packet_size", 0 },
                { "frame_size_ms", 20.F } });
        dec.init(std::make_shared<gr::Sequence>());
        dec.start();
        gnuradio4::opus::OpusEncoderCc enc(
            gr::property_map{ { "name", std::string("e4") }, { "sample_rate", dec.sample_rate.value }, { "channels", dec.channels.value },
                { "bitrate", 64000 }, { "application", std::string("audio") }, { "frame_size_ms", dec.frame_size_ms.value } });
        enc.init(std::make_shared<gr::Sequence>());
        enc.start();
        const int             sp = gnuradio4::opus::detail::frameSamplesPerChannel(48000, 20.F);
        std::vector<float>    pcm(static_cast<std::size_t>(sp), 0.F);
        gr::MsgPortIn          encSink;
        const std::vector<std::uint8_t> pdu = pduBytesFromEncoderFrame(enc, encSink, pcm.data(), pcm.size());

        std::vector<float> out(static_cast<std::size_t>(sp));
        gr::MsgPortOut     toDec;
        expect(toDec.connect(dec.pdu_in).has_value());
        pushPduMessage(toDec, std::vector<std::uint8_t>(pdu));
        dec.processScheduledMessages();
        expect(eq(stN(dec.processBulk(std::span<float>(out.data(), out.size()))), stN(gr::work::Status::OK)));
        for (float v : out) {
            expect(std::isfinite(v));
            expect(le(std::fabs(v), 1.01F));
        }
        dec.stop();
        enc.stop();
    };

    "round_trip_sine_spectral_peak"_test = [] {
        constexpr double              sampleRate = 48000.0;
        constexpr double              toneHz     = 1000.0;
        gnuradio4::opus::OpusDecoderCc dec(
            gr::property_map{ { "name", std::string("d5") }, { "sample_rate", 48000 }, { "channels", 1 }, { "packet_size", 0 },
                { "frame_size_ms", 20.F } });
        dec.init(std::make_shared<gr::Sequence>());
        dec.start();
        gnuradio4::opus::OpusEncoderCc enc(
            gr::property_map{ { "name", std::string("e5") }, { "sample_rate", dec.sample_rate.value }, { "channels", dec.channels.value },
                { "bitrate", 128000 }, { "application", std::string("audio") }, { "frame_size_ms", dec.frame_size_ms.value } });
        enc.init(std::make_shared<gr::Sequence>());
        enc.start();
        const int sp =
            gnuradio4::opus::detail::frameSamplesPerChannel(enc.sample_rate.value, enc.frame_size_ms.value);
        std::vector<float> pcm(static_cast<std::size_t>(sp));
        for (int n = 0; n < sp; ++n) {
            pcm[static_cast<std::size_t>(n)] = 0.2F * std::sin(2.F * std::numbers::pi_v<float> * (1000.F / 48000.F)
                * static_cast<float>(n));
        }
        gr::MsgPortIn                   encSink;
        const std::vector<std::uint8_t> pdu = pduBytesFromEncoderFrame(enc, encSink, pcm.data(), pcm.size());

        std::vector<float> out(static_cast<std::size_t>(sp));
        gr::MsgPortOut     toDec;
        expect(toDec.connect(dec.pdu_in).has_value());
        pushPduMessage(toDec, std::vector<std::uint8_t>(pdu));
        dec.processScheduledMessages();
        expect(eq(stN(dec.processBulk(std::span<float>(out.data(), out.size()))), stN(gr::work::Status::OK)));

        const double frac = toneEnergyFraction(std::span<const float>(out.data(), out.size()), toneHz, sampleRate);
        expect(gt(frac, 0.03));

        dec.stop();
        enc.stop();
    };
};

int main() {
    return boost::ut::cfg<boost::ut::override>.run();
}
