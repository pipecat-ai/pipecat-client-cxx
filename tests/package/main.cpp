//
// Copyright (c) 2024, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#include "rtvi.h"

int main() {
    auto message = rtvi::RTVIMessage::client_ready();
    return message["type"] == "client-ready" ? 0 : 1;
}
