//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#include "frames.h"

#include <stdexcept>
#include <string_view>

namespace pipecat::websocket {

namespace {

// Wire types.
const uint32_t VARINT = 0;
const uint32_t FIXED64 = 1;
const uint32_t LENGTH_DELIMITED = 2;
const uint32_t FIXED32 = 5;

// Fields of Frame (a oneof).
const uint32_t FRAME_AUDIO = 2;
const uint32_t FRAME_MESSAGE = 4;
const uint32_t FRAME_INTERRUPTION = 5;

// Fields of AudioRawFrame.
const uint32_t AUDIO_AUDIO = 3;
const uint32_t AUDIO_SAMPLE_RATE = 4;
const uint32_t AUDIO_NUM_CHANNELS = 5;

// Fields of MessageFrame.
const uint32_t MESSAGE_DATA = 1;

void put_varint(std::string& out, uint64_t value) {
    while (value >= 0x80) {
        out.push_back(static_cast<char>((value & 0x7f) | 0x80));
        value >>= 7;
    }
    out.push_back(static_cast<char>(value));
}

void put_tag(std::string& out, uint32_t field, uint32_t wire_type) {
    put_varint(out, (static_cast<uint64_t>(field) << 3) | wire_type);
}

void put_bytes(std::string& out, uint32_t field, std::string_view bytes) {
    put_tag(out, field, LENGTH_DELIMITED);
    put_varint(out, bytes.size());
    out.append(bytes);
}

void put_uint(std::string& out, uint32_t field, uint64_t value) {
    put_tag(out, field, VARINT);
    put_varint(out, value);
}

// Reads the fields of a message, one at a time.
class Reader {
   public:
    explicit Reader(std::string_view data) : _data(data) {}

    bool done() const { return _pos == _data.size(); }

    // Reads a field's tag, as its number and wire type.
    void tag(uint32_t& field, uint32_t& wire_type) {
        uint64_t tag = varint();
        field = static_cast<uint32_t>(tag >> 3);
        wire_type = static_cast<uint32_t>(tag & 7);
    }

    uint64_t varint() {
        uint64_t value = 0;
        for (int shift = 0; shift < 64; shift += 7) {
            uint8_t byte = static_cast<uint8_t>(take(1)[0]);
            value |= static_cast<uint64_t>(byte & 0x7f) << shift;
            if ((byte & 0x80) == 0) {
                return value;
            }
        }
        throw std::runtime_error("Invalid frame: varint too long");
    }

    std::string_view bytes() { return take(varint()); }

    void skip(uint32_t wire_type) {
        switch (wire_type) {
        case VARINT:
            varint();
            break;
        case FIXED64:
            take(8);
            break;
        case LENGTH_DELIMITED:
            bytes();
            break;
        case FIXED32:
            take(4);
            break;
        default:
            throw std::runtime_error("Invalid frame: unknown wire type");
        }
    }

   private:
    std::string_view take(uint64_t size) {
        if (size > _data.size() - _pos) {
            throw std::runtime_error("Invalid frame: truncated");
        }
        std::string_view result = _data.substr(_pos, size);
        _pos += size;
        return result;
    }

    std::string_view _data;
    size_t _pos = 0;
};

MessageFrame decode_message(std::string_view data) {
    MessageFrame frame;
    Reader reader(data);
    while (!reader.done()) {
        uint32_t field, wire_type;
        reader.tag(field, wire_type);
        if (field == MESSAGE_DATA && wire_type == LENGTH_DELIMITED) {
            frame.data = std::string(reader.bytes());
        } else {
            reader.skip(wire_type);
        }
    }
    return frame;
}

AudioFrame decode_audio(std::string_view data) {
    AudioFrame frame;
    Reader reader(data);
    while (!reader.done()) {
        uint32_t field, wire_type;
        reader.tag(field, wire_type);
        if (field == AUDIO_AUDIO && wire_type == LENGTH_DELIMITED) {
            std::string_view audio = reader.bytes();
            if (audio.size() % 2 != 0) {
                throw std::runtime_error("Invalid frame: odd audio size");
            }
            frame.samples.resize(audio.size() / 2);
            for (size_t i = 0; i < frame.samples.size(); ++i) {
                // Little-endian.
                auto low = static_cast<uint8_t>(audio[2 * i]);
                auto high = static_cast<uint8_t>(audio[2 * i + 1]);
                frame.samples[i] = static_cast<int16_t>(low | (high << 8));
            }
        } else if (field == AUDIO_SAMPLE_RATE && wire_type == VARINT) {
            frame.sample_rate = static_cast<uint32_t>(reader.varint());
        } else if (field == AUDIO_NUM_CHANNELS && wire_type == VARINT) {
            frame.num_channels = static_cast<uint32_t>(reader.varint());
        } else {
            reader.skip(wire_type);
        }
    }
    if (frame.sample_rate == 0 || frame.num_channels == 0) {
        throw std::runtime_error("Invalid frame: audio without a format");
    }
    if (frame.samples.size() % frame.num_channels != 0) {
        throw std::runtime_error("Invalid frame: partial audio frame");
    }
    return frame;
}

}  // namespace

std::string encode_message(const std::string& json) {
    std::string message;
    put_bytes(message, MESSAGE_DATA, json);

    std::string frame;
    put_bytes(frame, FRAME_MESSAGE, message);
    return frame;
}

std::string encode_audio(
        const int16_t* frames,
        size_t num_frames,
        uint32_t sample_rate,
        uint32_t num_channels
) {
    size_t num_samples = num_frames * num_channels;
    std::string samples(num_samples * 2, '\0');
    for (size_t i = 0; i < num_samples; ++i) {
        // Little-endian.
        auto sample = static_cast<uint16_t>(frames[i]);
        samples[2 * i] = static_cast<char>(sample & 0xff);
        samples[2 * i + 1] = static_cast<char>(sample >> 8);
    }

    std::string audio;
    put_bytes(audio, AUDIO_AUDIO, samples);
    put_uint(audio, AUDIO_SAMPLE_RATE, sample_rate);
    put_uint(audio, AUDIO_NUM_CHANNELS, num_channels);

    std::string frame;
    put_bytes(frame, FRAME_AUDIO, audio);
    return frame;
}

Frame decode(const void* data, size_t size) {
    Reader reader(std::string_view(static_cast<const char*>(data), size));
    // A oneof: the last field wins.
    Frame frame = OtherFrame {};
    while (!reader.done()) {
        uint32_t field, wire_type;
        reader.tag(field, wire_type);
        if (wire_type != LENGTH_DELIMITED) {
            reader.skip(wire_type);
            continue;
        }
        std::string_view body = reader.bytes();
        switch (field) {
        case FRAME_MESSAGE:
            frame = decode_message(body);
            break;
        case FRAME_AUDIO:
            frame = decode_audio(body);
            break;
        case FRAME_INTERRUPTION:
            frame = InterruptionFrame {};
            break;
        default:
            frame = OtherFrame {};
            break;
        }
    }
    return frame;
}

}  // namespace pipecat::websocket
