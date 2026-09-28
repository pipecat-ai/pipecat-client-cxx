//
// Copyright (c) 2024, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#include <pipecat/pipecat.h>

#include <string>

int main() {
    using namespace pipecat::rtvi;

    auto message = Message::client_ready(default_about_client());
    bool ok = message.type == "client-ready" &&
              message.data["about"]["library_version"] == PIPECAT_VERSION;
    return ok ? 0 : 1;
}
