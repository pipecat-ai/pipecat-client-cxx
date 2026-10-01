//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#include "audio_buffer.h"

#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <vector>

using namespace pipecat;
using Samples = std::vector<int16_t>;

namespace {

Samples read(AudioBuffer& buffer, size_t num_frames, size_t channels) {
    Samples samples(num_frames * channels);
    size_t read = buffer.read(samples.data(), num_frames);
    samples.resize(read * channels);
    return samples;
}

}  // namespace

TEST(AudioBuffer, ReadsWhatWasWritten) {
    AudioBuffer buffer(1, 100);
    buffer.open();

    Samples samples {1, 2, 3, 4};
    buffer.write(samples.data(), 4, 1);

    EXPECT_EQ(read(buffer, 3, 1), (Samples {1, 2, 3}));
    EXPECT_EQ(read(buffer, 1, 1), (Samples {4}));
}

TEST(AudioBuffer, ConvertsChannels) {
    AudioBuffer stereo(2, 100);
    stereo.open();
    Samples mono_samples {1, 2};
    stereo.write(mono_samples.data(), 2, 1);
    EXPECT_EQ(read(stereo, 2, 2), (Samples {1, 1, 2, 2}));

    AudioBuffer mono(1, 100);
    mono.open();
    Samples stereo_samples {10, 20, -4, -8};
    mono.write(stereo_samples.data(), 2, 2);
    EXPECT_EQ(read(mono, 2, 1), (Samples {15, -6}));
}

TEST(AudioBuffer, DropsOldestAudioWhenFull) {
    AudioBuffer buffer(1, 3);
    buffer.open();

    Samples samples {1, 2, 3, 4, 5};
    buffer.write(samples.data(), 5, 1);

    EXPECT_EQ(read(buffer, 3, 1), (Samples {3, 4, 5}));
}

TEST(AudioBuffer, ReadWaitsForEnoughAudio) {
    AudioBuffer buffer(1, 100);
    buffer.open();

    auto reader =
            std::async(std::launch::async, [&] { return read(buffer, 4, 1); });

    Samples samples {1, 2};
    buffer.write(samples.data(), 2, 1);
    EXPECT_EQ(
            reader.wait_for(std::chrono::milliseconds(50)),
            std::future_status::timeout
    );

    buffer.write(samples.data(), 2, 1);
    EXPECT_EQ(reader.get(), (Samples {1, 2, 1, 2}));
}

TEST(AudioBuffer, CloseWakesUpReaders) {
    AudioBuffer buffer(1, 100);
    buffer.open();

    auto reader =
            std::async(std::launch::async, [&] { return read(buffer, 4, 1); });
    buffer.close();

    EXPECT_TRUE(reader.get().empty());
}

TEST(AudioBuffer, ClearDropsUnreadAudio) {
    AudioBuffer buffer(1, 100);
    buffer.open();
    Samples samples {1, 2};
    buffer.write(samples.data(), 2, 1);

    buffer.clear();
    Samples more {3};
    buffer.write(more.data(), 1, 1);
    EXPECT_EQ(read(buffer, 1, 1), (Samples {3}));
}

TEST(AudioBuffer, IgnoresAudioWhileClosed) {
    AudioBuffer buffer(1, 100);
    Samples samples {1, 2};
    buffer.write(samples.data(), 2, 1);

    buffer.open();
    buffer.write(samples.data(), 2, 1);
    buffer.close();
    EXPECT_TRUE(read(buffer, 2, 1).empty());

    // Opening again starts from scratch.
    buffer.open();
    Samples more {3};
    buffer.write(more.data(), 1, 1);
    EXPECT_EQ(read(buffer, 1, 1), (Samples {3}));
}
