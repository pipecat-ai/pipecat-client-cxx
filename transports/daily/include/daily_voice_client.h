//
// Copyright (c) 2024, Daily
//

#ifndef DAILY_VOICE_CLIENT_H
#define DAILY_VOICE_CLIENT_H

#include "rtvi.h"

#include "daily_transport.h"

namespace rtvi {

class DailyVoiceClient : public RTVIClient {
   public:
    explicit DailyVoiceClient(const RTVIClientOptions& options);

    explicit DailyVoiceClient(
            const RTVIClientOptions& options,
            const DailyTransportParams& params
    );

    virtual ~DailyVoiceClient() override;

   private:
    std::unique_ptr<DailyTransport> _transport;
};

}  // namespace rtvi

#endif
