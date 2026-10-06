//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#include "params.h"

#include <pipecat/errors.h>

#include <gtest/gtest.h>

using namespace pipecat;
using namespace pipecat::smallwebrtc;
using nlohmann::json;

namespace {

APIRequest start_request(const std::string& endpoint) {
    APIRequest request;
    request.endpoint = endpoint;
    request.headers = {{"Authorization", "Bearer secret"}};
    request.request_data = {{"transport", "webrtc"}};
    return request;
}

}  // namespace

TEST(SessionOfferUrl, ReplacesStart) {
    EXPECT_EQ(
            session_offer_url("http://localhost:7860/start", "1234"),
            "http://localhost:7860/sessions/1234/api/offer"
    );
    EXPECT_EQ(
            session_offer_url(
                    "https://api.pipecat.daily.co/v1/public/agent/start", "1234"
            ),
            "https://api.pipecat.daily.co/v1/public/agent/sessions/1234/api/"
            "offer"
    );
}

TEST(SessionOfferUrl, KeepsTheQuery) {
    EXPECT_EQ(
            session_offer_url("http://localhost:7860/start?a=1#b", "1234"),
            "http://localhost:7860/sessions/1234/api/offer?a=1#b"
    );
}

TEST(SessionOfferUrl, OnlyLooksAtThePath) {
    EXPECT_EQ(
            session_offer_url("https://start.example.com/start", "1234"),
            "https://start.example.com/sessions/1234/api/offer"
    );
    EXPECT_THROW(
            session_offer_url("http://start", "1234"),
            InvalidTransportParamsError
    );
    EXPECT_THROW(
            session_offer_url("http://example.com/restart", "1234"),
            InvalidTransportParamsError
    );
    EXPECT_THROW(
            session_offer_url("http://example.com/start/now", "1234"),
            InvalidTransportParamsError
    );
}

TEST(ConnectionParams, SessionFromStartBot) {
    auto params = parse_connection_params(
            {{"sessionId", "1234"}},
            start_request("http://localhost:7860/start")
    );
    EXPECT_EQ(
            params.offer_url, "http://localhost:7860/sessions/1234/api/offer"
    );
    EXPECT_EQ(params.headers.at("Authorization"), "Bearer secret");
    // The start endpoint already has the bot's data.
    EXPECT_TRUE(params.request_data.is_null());
    EXPECT_TRUE(params.ice_servers.empty());
}

TEST(ConnectionParams, SessionNeedsStartBot) {
    EXPECT_THROW(
            parse_connection_params({{"sessionId", "1234"}}, std::nullopt),
            InvalidTransportParamsError
    );
}

TEST(ConnectionParams, RequestParams) {
    json params = {
            {"webrtcRequestParams",
             {{"endpoint", "http://localhost:7860/api/offer"},
              {"headers", {{"Authorization", "Bearer other"}}},
              {"requestData", {{"voice", "calm"}}}}},
            // Ignored, since the request params win.
            {"sessionId", "1234"},
    };
    auto result = parse_connection_params(
            params, start_request("http://localhost:7860/start")
    );
    EXPECT_EQ(result.offer_url, "http://localhost:7860/api/offer");
    EXPECT_EQ(result.headers.at("Authorization"), "Bearer other");
    EXPECT_EQ(result.request_data, json({{"voice", "calm"}}));
}

TEST(ConnectionParams, SnakeCase) {
    json params = {
            {"webrtc_request_params",
             {{"endpoint", "http://localhost:7860/api/offer"},
              {"request_data", {{"voice", "calm"}}}}},
            {"ice_config",
             {{"ice_servers", {{{"urls", "stun:stun.l.google.com:19302"}}}}}},
    };
    auto result = parse_connection_params(params, std::nullopt);
    EXPECT_EQ(result.offer_url, "http://localhost:7860/api/offer");
    EXPECT_EQ(result.request_data, json({{"voice", "calm"}}));
    ASSERT_EQ(result.ice_servers.size(), 1u);

    result = parse_connection_params(
            {{"session_id", "1234"}},
            start_request("http://localhost:7860/start")
    );
    EXPECT_EQ(
            result.offer_url, "http://localhost:7860/sessions/1234/api/offer"
    );
}

TEST(ConnectionParams, IceServers) {
    json params = {
            {"sessionId", "1234"},
            {"iceConfig",
             {{"iceServers",
               {{{"urls", "stun:stun.l.google.com:19302"}},
                {{"urls",
                  {"turn:turn.example.com:3478",
                   "turns:turn.example.com:5349"}},
                 {"username", "user"},
                 {"credential", "pass"}}}}}},
    };
    auto result = parse_connection_params(
            params, start_request("http://localhost:7860/start")
    );
    ASSERT_EQ(result.ice_servers.size(), 3u);

    const auto& stun = result.ice_servers[0];
    EXPECT_EQ(stun.type, rtc::IceServer::Type::Stun);
    EXPECT_EQ(stun.hostname, "stun.l.google.com");
    EXPECT_EQ(stun.port, 19302);

    const auto& turn = result.ice_servers[1];
    EXPECT_EQ(turn.type, rtc::IceServer::Type::Turn);
    EXPECT_EQ(turn.relayType, rtc::IceServer::RelayType::TurnUdp);
    EXPECT_EQ(turn.hostname, "turn.example.com");
    EXPECT_EQ(turn.username, "user");
    EXPECT_EQ(turn.password, "pass");

    const auto& turns = result.ice_servers[2];
    EXPECT_EQ(turns.relayType, rtc::IceServer::RelayType::TurnTls);
    EXPECT_EQ(turns.port, 5349);
    EXPECT_EQ(turns.username, "user");
}

TEST(ConnectionParams, Invalid) {
    auto start = start_request("http://localhost:7860/start");
    for (const char* params: {
                 R"(null)",
                 R"("http://localhost:7860/api/offer")",
                 R"({})",
                 R"({"sessionId": 1234})",
                 R"({"webrtcRequestParams": "http://localhost/api/offer"})",
                 R"({"webrtcRequestParams": {}})",
                 R"({"webrtcRequestParams": {"endpoint": "http://localhost/api/offer", "headers": {"Authorization": 1}}})",
                 R"({"sessionId": "1234", "iceConfig": "stun:x"})",
                 R"({"sessionId": "1234", "iceConfig": {"iceServers": [{"urls": 1}]}})",
                 R"({"sessionId": "1234", "iceConfig": {"iceServers": [{"urls": "http://x"}]}})",
         }) {
        EXPECT_THROW(
                parse_connection_params(json::parse(params), start),
                InvalidTransportParamsError
        ) << params;
    }
}
