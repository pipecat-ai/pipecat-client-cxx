//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#include "session.h"

#include <pipecat/errors.h>

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace pipecat;
using namespace pipecat::daily;
using nlohmann::json;
using Events = std::vector<std::string>;

namespace {

// Records observer calls as strings, e.g. "bot-connected:bot".
class Observer : public TransportObserver {
   public:
    Events events;

    void on_transport_message(const json& message) override {
        events.push_back("message:" + message.value("type", ""));
    }
    void on_bot_connected(const Participant& bot) override {
        events.push_back("bot-connected:" + bot.id);
    }
    void on_bot_disconnected(const Participant& bot) override {
        events.push_back("bot-disconnected:" + bot.id);
    }
    void on_participant_joined(const Participant& participant) override {
        events.push_back("participant-joined:" + participant.id);
    }
    void on_participant_left(const Participant& participant) override {
        events.push_back("participant-left:" + participant.id);
    }
    void on_transport_error(const std::string& error, bool fatal) override {
        events.push_back("error:" + error + (fatal ? ":fatal" : ""));
    }
    void on_transport_disconnected() override {
        events.push_back("disconnected");
    }
};

json participant(
        const std::string& id,
        bool local = false,
        const std::string& microphone = "off"
) {
    return {
            {"id", id},
            {"info", {{"isLocal", local}, {"userName", id + "-name"}}},
            {"media", {{"microphone", {{"state", microphone}}}}},
    };
}

json participant_event(const std::string& action, const json& participant) {
    return {{"action", action}, {"participant", participant}};
}

struct TestSession {
    Observer observer;
    std::vector<std::string> sent;
    std::vector<std::string> captured;
    Session session {
            &observer,
            [this](std::string message) { sent.push_back(std::move(message)); },
            [this](const std::string& bot_id) { captured.push_back(bot_id); }
    };
};

}  // namespace

//
// Connection params
//

TEST(ConnectionParams, UrlAndToken) {
    auto params = parse_connection_params(
            {{"url", "https://example.daily.co/room"}, {"token", "secret"}}
    );
    EXPECT_EQ(params.url, "https://example.daily.co/room");
    EXPECT_EQ(params.token, "secret");
}

TEST(ConnectionParams, StartEndpointNames) {
    auto params = parse_connection_params(
            {{"dailyRoom", "https://example.daily.co/room"},
             {"dailyToken", "secret"},
             {"sessionId", "123"}}
    );
    EXPECT_EQ(params.url, "https://example.daily.co/room");
    EXPECT_EQ(params.token, "secret");

    params = parse_connection_params(
            {{"room_url", "https://example.daily.co/room"}}
    );
    EXPECT_EQ(params.url, "https://example.daily.co/room");
    EXPECT_EQ(params.token, "");
}

TEST(ConnectionParams, NeedRoomUrl) {
    EXPECT_THROW(
            parse_connection_params({{"token", "secret"}}),
            InvalidTransportParamsError
    );
    EXPECT_THROW(parse_connection_params(nullptr), InvalidTransportParamsError);
    EXPECT_THROW(
            parse_connection_params("https://example.daily.co/room"),
            InvalidTransportParamsError
    );
}

//
// Participants
//

TEST(Session, FirstRemoteParticipantIsTheBot) {
    TestSession test;

    test.session.handle_event(
            participant_event("participant-joined", participant("me", true))
    );
    test.session.handle_event(
            participant_event("participant-joined", participant("bot"))
    );
    test.session.handle_event(
            participant_event("participant-joined", participant("user"))
    );
    test.session.handle_event(
            participant_event("participant-left", participant("user"))
    );
    test.session.handle_event(
            participant_event("participant-left", participant("bot"))
    );

    EXPECT_EQ(
            test.observer.events,
            (Events {
                    "bot-connected:bot",
                    "participant-joined:user",
                    "participant-left:user",
                    "bot-disconnected:bot",
            })
    );
}

TEST(Session, CapturesBotAudio) {
    TestSession test;

    test.session.handle_event(
            participant_event("participant-joined", participant("bot"))
    );
    test.session.handle_event(participant_event(
            "participant-updated", participant("bot", false, "playable")
    ));
    test.session.handle_event(
            participant_event("participant-joined", participant("user"))
    );
    EXPECT_EQ(test.captured, (std::vector<std::string> {"bot"}));

    // A new bot after the first one left.
    test.session.handle_event(
            participant_event("participant-left", participant("bot"))
    );
    test.session.handle_event(
            participant_event("participant-joined", participant("bot-2"))
    );
    EXPECT_EQ(test.captured, (std::vector<std::string> {"bot", "bot-2"}));
}

TEST(Session, ParticipantDetails) {
    Participant bot;
    class : public TransportObserver {
       public:
        Participant* bot;
        void on_transport_message(const json&) override {}
        void on_bot_connected(const Participant& participant) override {
            *bot = participant;
        }
        void on_bot_disconnected(const Participant&) override {}
        void on_participant_joined(const Participant&) override {}
        void on_participant_left(const Participant&) override {}
        void on_transport_error(const std::string&, bool) override {}
        void on_transport_disconnected() override {}
    } observer;
    observer.bot = &bot;
    Session session(&observer, [](std::string) {}, [](const std::string&) {});

    session.handle_event(
            participant_event("participant-joined", participant("bot"))
    );

    EXPECT_EQ(bot.id, "bot");
    EXPECT_EQ(bot.name, "bot-name");
    EXPECT_FALSE(bot.local);
}

//
// client-ready
//

TEST(Session, SendsReadyMessageWhenBotAudioPlays) {
    TestSession test;
    test.session.handle_event(
            participant_event("participant-joined", participant("bot"))
    );

    test.session.send_ready_message("client-ready");
    EXPECT_TRUE(test.sent.empty());

    test.session.handle_event(participant_event(
            "participant-updated", participant("bot", false, "playable")
    ));
    test.session.handle_event(participant_event(
            "participant-updated", participant("bot", false, "playable")
    ));

    EXPECT_EQ(test.sent, (std::vector<std::string> {"client-ready"}));
}

TEST(Session, SendsReadyMessageRightAwayIfBotAudioPlays) {
    TestSession test;
    test.session.handle_event(participant_event(
            "participant-joined", participant("bot", false, "playable")
    ));

    test.session.send_ready_message("client-ready");

    EXPECT_EQ(test.sent, (std::vector<std::string> {"client-ready"}));
}

TEST(Session, IgnoresOtherParticipantsAudio) {
    TestSession test;
    test.session.handle_event(
            participant_event("participant-joined", participant("bot"))
    );
    test.session.send_ready_message("client-ready");

    test.session.handle_event(participant_event(
            "participant-joined", participant("user", false, "playable")
    ));

    EXPECT_TRUE(test.sent.empty());
}

//
// Messages
//

TEST(Session, ForwardsRtviMessages) {
    TestSession test;

    test.session.handle_event(
            {{"action", "app-message"},
             {"msgData", {{"label", "rtvi-ai"}, {"type", "bot-ready"}}}}
    );
    test.session.handle_event(
            {{"action", "app-message"},
             {"msgData", {{"label", "other"}, {"type", "chat"}}}}
    );

    EXPECT_EQ(test.observer.events, (Events {"message:bot-ready"}));
}

//
// Leaving and errors
//

TEST(Session, ReportsUnexpectedDisconnection) {
    TestSession test;

    test.session.handle_event(
            {{"action", "call-state-updated"}, {"state", "joined"}}
    );
    test.session.handle_event(
            {{"action", "call-state-updated"}, {"state", "left"}}
    );

    EXPECT_TRUE(test.session.left());
    EXPECT_EQ(test.observer.events, (Events {"disconnected"}));
}

TEST(Session, ReportsDisconnectionError) {
    TestSession test;

    test.session.handle_event(
            {{"action", "call-state-updated"},
             {"state", "left"},
             {"error", "Room closed"}}
    );

    EXPECT_EQ(
            test.observer.events,
            (Events {"error:Room closed:fatal", "disconnected"})
    );
}

TEST(Session, IgnoresEventsWhileLeaving) {
    TestSession test;
    test.session.handle_event(
            participant_event("participant-joined", participant("bot"))
    );

    test.session.set_leaving();
    test.session.handle_event(
            participant_event("participant-left", participant("bot"))
    );
    test.session.handle_event(
            {{"action", "app-message"},
             {"msgData", {{"label", "rtvi-ai"}, {"type", "bot-output"}}}}
    );
    test.session.handle_event(
            {{"action", "call-state-updated"}, {"state", "left"}}
    );

    EXPECT_FALSE(test.session.left());
    EXPECT_EQ(test.observer.events, (Events {"bot-connected:bot"}));
}

TEST(Session, ReportsErrors) {
    TestSession test;

    test.session.handle_event({{"action", "error"}, {"message", "Ejected"}});

    EXPECT_EQ(test.observer.events, (Events {"error:Ejected:fatal"}));
}

TEST(Session, IgnoresMalformedEvents) {
    TestSession test;

    test.session.handle_event({{"action", "participant-joined"}});
    test.session.handle_event({{"action", 5}});
    test.session.handle_event(json::array());
    test.session.handle_event({{"action", "app-message"}, {"msgData", "text"}});

    EXPECT_TRUE(test.observer.events.empty());
}
