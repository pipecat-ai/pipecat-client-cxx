//
// Copyright (c) 2024, Daily
//

#include "daily_voice_client.h"

using namespace rtvi;

DailyVoiceClient::DailyVoiceClient(const RTVIClientOptions& options)
    : RTVIClient(options, std::make_unique<DailyTransport>(options, this)) {}

DailyVoiceClient::DailyVoiceClient(
        const RTVIClientOptions& options,
        const DailyTransportParams& params
)
    : RTVIClient(
              options,
              std::make_unique<DailyTransport>(options, params, this)
      ) {}

DailyVoiceClient::~DailyVoiceClient() {}
