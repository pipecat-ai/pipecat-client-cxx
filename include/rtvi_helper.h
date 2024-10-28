//
// Copyright (c) 2024, Daily
//

#ifndef RTVI_HELPER_H
#define RTVI_HELPER_H

#include "rtvi_transport.h"

#include <string>
#include <vector>

namespace rtvi {

class RTVIHelper {
   public:
    virtual ~RTVIHelper() = default;

    virtual const std::vector<std::string>& supported_messages() = 0;

    virtual void
    handle_message(RTVITransport* transport, const nlohmann::json& action) = 0;
};

}  // namespace rtvi

#endif
