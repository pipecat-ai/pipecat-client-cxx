//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#ifndef PIPECAT_HTTP_H
#define PIPECAT_HTTP_H

#include "pipecat/client.h"

#include <functional>
#include <string>

namespace pipecat {

struct HttpResponse {
    long status = 0;
    std::string body;
};

// POSTs `request.request_data` as JSON. Returns the response whatever its
// status. Throws StartBotError if there's no response, and PipecatError if
// `cancelled` returns true while waiting.
HttpResponse
http_post(const APIRequest& request, const std::function<bool()>& cancelled);

}  // namespace pipecat

#endif
