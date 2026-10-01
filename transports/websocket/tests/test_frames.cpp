//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#include "frames.h"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

using namespace pipecat::websocket;

namespace {

std::string from_hex(const std::string& hex) {
    std::string bytes;
    for (size_t i = 0; i < hex.size(); i += 2) {
        bytes.push_back(
                static_cast<char>(std::stoi(hex.substr(i, 2), nullptr, 16))
        );
    }
    return bytes;
}

std::string to_hex(const std::string& bytes) {
    static const char* digits = "0123456789abcdef";
    std::string hex;
    for (unsigned char byte: bytes) {
        hex.push_back(digits[byte >> 4]);
        hex.push_back(digits[byte & 0xf]);
    }
    return hex;
}

Frame decode_hex(const std::string& hex) {
    std::string bytes = from_hex(hex);
    return decode(bytes.data(), bytes.size());
}

}  // namespace

//
// What the transport sends. The expected bytes come from Pipecat's protobuf
// classes (frames_pb2), so the bot reads them back as the same frames.
//

TEST(Frames, EncodesMessages) {
    std::string frame =
            encode_message(R"({"label":"rtvi-ai","type":"client-ready"})");
    EXPECT_EQ(
            to_hex(frame),
            "222b0a297b226c6162656c223a22727476692d6169222c2274797065223a"
            "22636c69656e742d7265616479227d"
    );
}

TEST(Frames, EncodesAudio) {
    int16_t samples[] = {1, -1, 256};
    std::string frame = encode_audio(samples, 3, 16000, 1);
    EXPECT_EQ(to_hex(frame), "120d1a060100ffff000120807d2801");
}

TEST(Frames, EncodesAudioWithSeveralChannels) {
    std::vector<int16_t> samples(2000 * 2);
    for (size_t i = 0; i < samples.size(); ++i) {
        samples[i] = static_cast<int16_t>(i * 7 - 10000);
    }
    std::string frame = encode_audio(samples.data(), 2000, 48000, 2);

    auto audio = std::get<AudioFrame>(decode(frame.data(), frame.size()));
    EXPECT_EQ(audio.samples, samples);
    EXPECT_EQ(audio.sample_rate, 48000u);
    EXPECT_EQ(audio.num_channels, 2u);
}

//
// What the bot sends. The bytes come from Pipecat's ProtobufFrameSerializer,
// which also sets the frames' id and name.
//

TEST(Frames, DecodesMessages) {
    auto frame = decode_hex(
            "222b0a297b226c6162656c223a2022727476692d6169222c20227479706522"
            "3a2022626f742d7265616479227d"
    );
    EXPECT_EQ(
            std::get<MessageFrame>(frame).data,
            R"({"label": "rtvi-ai", "type": "bot-ready"})"
    );
}

TEST(Frames, DecodesAudio) {
    auto frame = decode_hex(
            "1225080212154f7574707574417564696f5261774672616d6523301a040100"
            "ffff20c0bb012801"
    );
    auto audio = std::get<AudioFrame>(frame);
    EXPECT_EQ(audio.samples, (std::vector<int16_t> {1, -1}));
    EXPECT_EQ(audio.sample_rate, 24000u);
    EXPECT_EQ(audio.num_channels, 1u);
}

TEST(Frames, DecodesInterruptions) {
    auto frame =
            decode_hex("2a1708031213496e74657272757074696f6e4672616d652330");
    EXPECT_TRUE(std::holds_alternative<InterruptionFrame>(frame));
}

TEST(Frames, IgnoresOtherFrames) {
    // A TextFrame.
    auto frame = decode_hex("0a130804120b546578744672616d6523301a026869");
    EXPECT_TRUE(std::holds_alternative<OtherFrame>(frame));

    EXPECT_TRUE(std::holds_alternative<OtherFrame>(decode("", 0)));
}

TEST(Frames, RejectsInvalidFrames) {
    // Truncated audio.
    EXPECT_THROW(
            decode_hex("120d1a060100ffff000120807d28"), std::runtime_error
    );
    // Audio with an odd number of bytes.
    EXPECT_THROW(decode_hex("12051a03010203"), std::runtime_error);
    // A length past the end.
    EXPECT_THROW(decode_hex("22ff01"), std::runtime_error);
    // An unknown wire type.
    EXPECT_THROW(decode_hex("0f"), std::runtime_error);
}
