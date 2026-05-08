// SPDX-License-Identifier: GPL-3.0-or-later
#include <boost/ut.hpp>

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/Message.hpp>
#include <gnuradio-4.0/Port.hpp>
#include <gnuradio-4.0/Sequence.hpp>
#include <gnuradio-4.0/Tag.hpp>
#include <gnuradio-4.0/Tensor.hpp>
#include <gnuradio-4.0/Value.hpp>
#include <gnuradio-4.0/opus/OpusEncoderCc.hpp>

#include <cmath>
#include <string>
#include <vector>

using namespace boost::ut;

namespace {

[[nodiscard]] constexpr int stN(gr::work::Status st) noexcept {
    return static_cast<int>(st);
}

void subscribeEncoderPdu(gnuradio4::opus::OpusEncoderCc& enc, gr::MsgPortIn& sink) {
    expect(enc.pdu_out.connect(sink).has_value());
}

[[nodiscard]] const gr::Tensor<std::uint8_t>* pduFromFirstMessage(gr::MsgPortIn& sink) {
    expect(eq(sink.streamReader().available(), 1UZ));
    auto                 readerSpan = sink.streamReader().template get<gr::SpanReleasePolicy::ProcessAll>(1UZ);
    const gr::Message& message       = readerSpan[0UZ];
    if (!message.data.has_value()) {
        return nullptr;
    }
    const gr::property_map&           body = message.data.value();
    const auto                        key = gr::convert_string_domain(std::string_view("pdu_bytes"));
    const auto                       it = body.find(key);
    if (it == body.end()) {
        return nullptr;
    }
    return it->second.get_if<gr::Tensor<std::uint8_t>>();
}

} // namespace

const suite<"OpusEncoder"> OpusEncoderSuite = [] {
    "valid_sample_rates_start_and_encode"_test = [] {
        constexpr int                                         rates[] = { 8000, 12000, 16000, 24000, 48000 };
        for (int sr : rates) {
            gnuradio4::opus::OpusEncoderCc enc(gr::property_map{ { "name", std::string("enc_sr") }, { "sample_rate", sr },
                { "channels", 1 }, { "bitrate", 64000 }, { "application", std::string("audio") }, { "frame_size_ms", 20.F } });
            enc.init(std::make_shared<gr::Sequence>());
            enc.start();
            gr::MsgPortIn sinkPdu;
            subscribeEncoderPdu(enc, sinkPdu);

            const int frameSpC = gnuradio4::opus::detail::frameSamplesPerChannel(sr, enc.frame_size_ms.value);
            const std::vector<float> pcm(static_cast<std::size_t>(frameSpC * enc.channels.value), 0.F);
            const gr::work::Status   st =
                enc.processBulk(std::span<const float>(pcm.data(), pcm.size()));

            expect(eq(stN(st), stN(gr::work::Status::OK)));
            const gr::Tensor<std::uint8_t>* pdu = pduFromFirstMessage(sinkPdu);
            expect(pdu != nullptr);
            expect(pdu->size() >= 1UZ);
            enc.stop();
        }
    };

    "mono_and_stereo_frames"_test = [] {
        for (int ch : { 1, 2 }) {
            gnuradio4::opus::OpusEncoderCc enc(
                gr::property_map{ { "name", std::string("enc_ch") }, { "sample_rate", 48000 }, { "channels", ch },
                    { "bitrate", 96000 }, { "application", std::string("audio") }, { "frame_size_ms", 20.F } });
            enc.init(std::make_shared<gr::Sequence>());
            enc.start();
            gr::MsgPortIn sinkPdu;
            subscribeEncoderPdu(enc, sinkPdu);
            const int                 spc   = gnuradio4::opus::detail::frameSamplesPerChannel(48000, 20.F);
            const std::size_t          total = gnuradio4::opus::detail::interleavedSampleCount(spc, ch);
            const std::vector<float> pcm(total, 0.F);
            const gr::work::Status  st =
                enc.processBulk(std::span<const float>(pcm.data(), pcm.size()));

            expect(eq(stN(st), stN(gr::work::Status::OK)));
            const gr::Tensor<std::uint8_t>* pdu = pduFromFirstMessage(sinkPdu);
            expect(pdu != nullptr);
            expect(ge(pdu->size(), 1UZ));

            enc.stop();
        }
    };

    "invalid_sample_rate_returns_error_when_processing"_test = [] {
        gnuradio4::opus::OpusEncoderCc enc(gr::property_map{ { "name", std::string("bad") }, { "sample_rate", 44100 }, { "channels", 1 },
            { "bitrate", 64000 }, { "application", std::string("audio") }, { "frame_size_ms", 20.F } });
        enc.init(std::make_shared<gr::Sequence>());
        enc.start();

        std::vector<float>       pcm(960U, 0.F);
        const gr::work::Status  st =
            enc.processBulk(std::span<float>(pcm.data(), pcm.size()));

        expect(eq(stN(st), stN(gr::work::Status::ERROR)));
        enc.stop();
    };

    "silence_emits_pdu_rfc6716_bounds"_test = [] {
        gnuradio4::opus::OpusEncoderCc enc(gr::property_map{ { "name", std::string("sil") }, { "sample_rate", 48000 }, { "channels", 1 },
            { "bitrate", 24000 }, { "application", std::string("voip") }, { "frame_size_ms", 20.F } });
        enc.init(std::make_shared<gr::Sequence>());
        enc.start();
        gr::MsgPortIn sinkPdu;
        subscribeEncoderPdu(enc, sinkPdu);
        std::vector<float>      pcm(static_cast<std::size_t>(
                                        gnuradio4::opus::detail::frameSamplesPerChannel(enc.sample_rate.value, enc.frame_size_ms.value)),
            0.F);
        expect(eq(stN(enc.processBulk(std::span<float>(pcm.data(), pcm.size()))), stN(gr::work::Status::OK)));
        const gr::Tensor<std::uint8_t>* pdu = pduFromFirstMessage(sinkPdu);
        expect(pdu != nullptr);
        expect(pdu->size() >= 1UZ);
        expect(pdu->size() <= 1275UZ);
        enc.stop();
    };

    "twenty_ms_48k_mono_consumes_960_samples"_test = [] {
        gnuradio4::opus::OpusEncoderCc enc(gr::property_map{ { "name", std::string("fs") }, { "sample_rate", 48000 }, { "channels", 1 },
            { "bitrate", 64000 }, { "application", std::string("audio") }, { "frame_size_ms", 20.F } });
        enc.init(std::make_shared<gr::Sequence>());
        enc.start();
        expect(eq(static_cast<long>(48000.F * 20.F / 1000.F + 0.5F), 960L));
        std::vector<float>      pcm(static_cast<std::size_t>(
                                        gnuradio4::opus::detail::frameSamplesPerChannel(enc.sample_rate.value, enc.frame_size_ms.value)),
            0.F);
        expect(eq(static_cast<long>(pcm.size()), 960L));
        expect(eq(stN(enc.processBulk(std::span<float>(pcm.data(), pcm.size()))), stN(gr::work::Status::OK)));
        enc.stop();
    };
};

int main() {
    return boost::ut::cfg<boost::ut::override>.run();
}
