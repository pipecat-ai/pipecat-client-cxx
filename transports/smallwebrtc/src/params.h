//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#ifndef PIPECAT_SMALLWEBRTC_PARAMS_H
#define PIPECAT_SMALLWEBRTC_PARAMS_H

#include <pipecat/client.h>

#include <nlohmann/json.hpp>
#include <rtc/configuration.hpp>

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace pipecat::smallwebrtc {

// Where to send the offer, and how to connect.
struct ConnectionParams {
    // URL of the offer endpoint.
    std::string offer_url;
    // HTTP headers of the offer.
    std::map<std::string, std::string> headers;
    // Data for the bot, sent with the offer. Null if there's none.
    nlohmann::json request_data;
    // STUN and TURN servers.
    std::vector<rtc::IceServer> ice_servers;
};

// Reads the params given to connect(). `start_request` is the request that
// started the bot, if start_bot() did. Throws InvalidTransportParamsError if
// they're wrong.
ConnectionParams parse_connection_params(
        const nlohmann::json& params,
        const std::optional<APIRequest>& start_request
);

// The offer URL of session `session_id`, started with `start_url`: that URL
// with the `/start` at the end of its path replaced by
// `/sessions/<session_id>/api/offer`. Throws InvalidTransportParamsError if
// its path doesn't end with `/start`.
std::string
session_offer_url(const std::string& start_url, const std::string& session_id);

}  // namespace pipecat::smallwebrtc

#endif
