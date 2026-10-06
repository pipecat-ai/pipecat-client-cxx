//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#include "params.h"

#include <pipecat/errors.h>

#include <exception>

using nlohmann::json;

namespace pipecat::smallwebrtc {

namespace {

// The value of `key` in `object`, in camel case or in snake case. Null if
// it's not there.
const json* find(const json& object, const char* camel, const char* snake) {
    for (const char* key: {camel, snake}) {
        auto it = object.find(key);
        if (it != object.end()) {
            return &*it;
        }
    }
    return nullptr;
}

const json* find(const json& object, const char* key) {
    return find(object, key, key);
}

std::string string_value(const json& value, const std::string& name) {
    if (!value.is_string()) {
        throw InvalidTransportParamsError("`" + name + "` must be a string");
    }
    return value.get<std::string>();
}

std::vector<std::string> ice_server_urls(const json& server) {
    const json* urls = find(server, "urls");
    if (urls == nullptr) {
        throw InvalidTransportParamsError("ICE servers need their `urls`");
    }
    if (urls->is_string()) {
        return {urls->get<std::string>()};
    }
    std::vector<std::string> result;
    if (urls->is_array()) {
        for (const json& url: *urls) {
            result.push_back(string_value(url, "urls"));
        }
    }
    if (result.empty()) {
        throw InvalidTransportParamsError(
                "An ICE server's `urls` must be a URL, or a list of them"
        );
    }
    return result;
}

rtc::IceServer ice_server(const std::string& url) {
    try {
        return rtc::IceServer(url);
    } catch (const std::exception&) {
        throw InvalidTransportParamsError("Invalid ICE server URL: " + url);
    }
}

std::vector<rtc::IceServer> ice_servers(const json& ice_config) {
    if (!ice_config.is_object()) {
        throw InvalidTransportParamsError("`iceConfig` must be a JSON object");
    }
    const json* servers = find(ice_config, "iceServers", "ice_servers");
    if (servers == nullptr) {
        return {};
    }
    if (!servers->is_array()) {
        throw InvalidTransportParamsError("`iceServers` must be a list");
    }

    std::vector<rtc::IceServer> result;
    for (const json& server: *servers) {
        if (!server.is_object()) {
            throw InvalidTransportParamsError(
                    "ICE servers must be JSON objects"
            );
        }
        const json* username = find(server, "username");
        const json* credential = find(server, "credential");
        for (const std::string& url: ice_server_urls(server)) {
            rtc::IceServer parsed = ice_server(url);
            if (username != nullptr) {
                parsed.username = string_value(*username, "username");
            }
            if (credential != nullptr) {
                parsed.password = string_value(*credential, "credential");
            }
            result.push_back(parsed);
        }
    }
    return result;
}

}  // namespace

ConnectionParams parse_connection_params(
        const json& params,
        const std::optional<APIRequest>& start_request
) {
    if (!params.is_object()) {
        throw InvalidTransportParamsError(
                "SmallWebRTC connection params must be a JSON object"
        );
    }

    ConnectionParams result;
    if (const json* request =
                find(params, "webrtcRequestParams", "webrtc_request_params")) {
        if (!request->is_object()) {
            throw InvalidTransportParamsError(
                    "`webrtcRequestParams` must be a JSON object"
            );
        }
        const json* endpoint = find(*request, "endpoint");
        if (endpoint != nullptr) {
            result.offer_url = string_value(*endpoint, "endpoint");
        }
        if (result.offer_url.empty()) {
            throw InvalidTransportParamsError(
                    "`webrtcRequestParams` needs the offer URL, in `endpoint`"
            );
        }
        if (const json* headers = find(*request, "headers")) {
            if (!headers->is_object()) {
                throw InvalidTransportParamsError(
                        "`headers` must be a JSON object"
                );
            }
            for (const auto& [name, value]: headers->items()) {
                result.headers[name] = string_value(value, name);
            }
        }
        if (const json* data = find(*request, "requestData", "request_data")) {
            result.request_data = *data;
        }
    } else if (const json* session_id =
                       find(params, "sessionId", "session_id")) {
        if (!start_request) {
            throw InvalidTransportParamsError(
                    "A `sessionId` only works after start_bot(), which tells "
                    "where to send the offer. Without it, use "
                    "`webrtcRequestParams`"
            );
        }
        result.offer_url = session_offer_url(
                start_request->endpoint, string_value(*session_id, "sessionId")
        );
        result.headers = start_request->headers;
    } else {
        throw InvalidTransportParamsError(
                "SmallWebRTC connection params need a `sessionId` from "
                "start_bot(), or `webrtcRequestParams`"
        );
    }

    if (const json* ice_config = find(params, "iceConfig", "ice_config")) {
        result.ice_servers = ice_servers(*ice_config);
    }
    return result;
}

std::string
session_offer_url(const std::string& start_url, const std::string& session_id) {
    // The path starts after the scheme and the host, and ends at the query or
    // the fragment.
    size_t scheme_end = start_url.find("://");
    size_t path_begin = start_url.find(
            '/', scheme_end == std::string::npos ? 0 : scheme_end + 3
    );
    size_t path_end = start_url.find_first_of("?#");
    if (path_end == std::string::npos) {
        path_end = start_url.size();
    }

    const std::string start = "/start";
    if (path_begin == std::string::npos || path_end < path_begin ||
        path_end - path_begin < start.size() ||
        start_url.compare(path_end - start.size(), start.size(), start) != 0) {
        throw InvalidTransportParamsError(
                "The start URL " + start_url +
                " doesn't end with /start, so the offer URL is unknown. Use "
                "`webrtcRequestParams`"
        );
    }

    return start_url.substr(0, path_end - start.size()) + "/sessions/" +
           session_id + "/api/offer" + start_url.substr(path_end);
}

}  // namespace pipecat::smallwebrtc
