//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

// Uses a small POSIX socket server, so these don't run on Windows.
#ifndef _WIN32

#include "fake_transport.h"
#include "recorder.h"

#include "pipecat/client.h"
#include "pipecat/errors.h"

#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cctype>
#include <chrono>
#include <future>
#include <string>
#include <thread>

using namespace pipecat;
using nlohmann::json;

namespace {

// Serves a single HTTP request on localhost. With `respond` false, it reads
// the request and never answers.
class HttpServer {
   public:
    HttpServer(int status, std::string body, bool respond = true)
        : _status(status), _body(std::move(body)), _respond(respond) {
        _socket = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in address {};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = 0;
        bind(_socket, reinterpret_cast<sockaddr*>(&address), sizeof(address));
        listen(_socket, 1);
        socklen_t length = sizeof(address);
        getsockname(_socket, reinterpret_cast<sockaddr*>(&address), &length);
        _port = ntohs(address.sin_port);
        _request = _request_promise.get_future();
        _thread = std::thread([this] { serve(); });
    }

    ~HttpServer() {
        if (!_accepted) {
            // Unblock accept() with a connection of our own.
            int client = socket(AF_INET, SOCK_STREAM, 0);
            sockaddr_in address {};
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            address.sin_port = htons(_port);
            connect(client,
                    reinterpret_cast<sockaddr*>(&address),
                    sizeof(address));
            close(client);
        }
        _thread.join();
        close(_socket);
    }

    std::string url() const {
        return "http://127.0.0.1:" + std::to_string(_port) + "/start";
    }

    // The raw request, once received.
    std::string request() {
        if (_request.wait_for(std::chrono::seconds(5)) !=
            std::future_status::ready) {
            return "";
        }
        return _request.get();
    }

   private:
    static size_t content_length(const std::string& headers) {
        std::string lower;
        for (char c: headers) {
            lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c
            )));
        }
        auto position = lower.find("content-length:");
        if (position == std::string::npos) {
            return 0;
        }
        return std::stoul(headers.substr(position + 15));
    }

    void serve() {
        int client = accept(_socket, nullptr, nullptr);
        _accepted = true;
        if (client < 0) {
            return;
        }

        std::string request;
        char buffer[4096];
        for (;;) {
            ssize_t size = recv(client, buffer, sizeof(buffer), 0);
            if (size <= 0) {
                break;
            }
            request.append(buffer, static_cast<size_t>(size));
            auto headers_end = request.find("\r\n\r\n");
            if (headers_end != std::string::npos &&
                request.size() >=
                        headers_end + 4 +
                                content_length(request.substr(0, headers_end)
                                )) {
                break;
            }
        }
        _request_promise.set_value(request);

        if (_respond) {
            std::string response = "HTTP/1.1 " + std::to_string(_status) +
                                   " Status\r\n"
                                   "Content-Type: application/json\r\n"
                                   "Content-Length: " +
                                   std::to_string(_body.size()) +
                                   "\r\n"
                                   "Connection: close\r\n\r\n" +
                                   _body;
            send(client, response.data(), response.size(), 0);
        } else {
            // Wait until the client gives up and closes the connection.
            while (recv(client, buffer, sizeof(buffer), 0) > 0) {
            }
        }
        close(client);
    }

    int _status;
    std::string _body;
    bool _respond;
    int _socket;
    int _port;
    std::atomic<bool> _accepted {false};
    std::promise<std::string> _request_promise;
    std::future<std::string> _request;
    std::thread _thread;
};

std::unique_ptr<PipecatClient>
make_client(Recorder& recorder, FakeTransport** transport = nullptr) {
    auto fake = std::make_unique<FakeTransport>();
    if (transport != nullptr) {
        *transport = fake.get();
    }
    PipecatClientOptions options;
    options.transport = std::move(fake);
    options.callbacks = &recorder;
    return std::make_unique<PipecatClient>(std::move(options));
}

}  // namespace

TEST(StartBot, PostsRequestAndReturnsResponse) {
    HttpServer server(200, R"({"room_url": "https://example.com/room"})");
    Recorder recorder;
    auto client = make_client(recorder);

    APIRequest request;
    request.endpoint = server.url();
    request.headers = {{"Authorization", "Bearer secret"}};
    request.request_data = {{"createDailyRoom", true}};
    auto response = client->start_bot(request);

    EXPECT_EQ(response, json({{"room_url", "https://example.com/room"}}));
    EXPECT_EQ(client->state(), TransportState::Authenticated);
    ASSERT_TRUE(recorder.wait_for(
            R"(bot-started:{"room_url":"https://example.com/room"})"
    ));

    auto raw = server.request();
    EXPECT_EQ(raw.rfind("POST /start HTTP/1.1\r\n", 0), 0u) << raw;
    EXPECT_NE(raw.find("Authorization: Bearer secret\r\n"), std::string::npos);
    EXPECT_NE(
            raw.find("Content-Type: application/json\r\n"), std::string::npos
    );
    EXPECT_NE(raw.find(R"({"createDailyRoom":true})"), std::string::npos);
}

TEST(StartBot, ErrorResponse) {
    HttpServer server(401, R"({"info": "Invalid API key"})");
    Recorder recorder;
    auto client = make_client(recorder);

    APIRequest request;
    request.endpoint = server.url();
    try {
        client->start_bot(request);
        FAIL() << "start_bot() didn't throw";
    } catch (const StartBotError& e) {
        EXPECT_EQ(e.status(), 401);
        EXPECT_STREQ(e.what(), "Invalid API key");
    }

    EXPECT_EQ(client->state(), TransportState::Error);
    ASSERT_TRUE(recorder.wait_for("error:Invalid API key:fatal"));
}

TEST(StartBot, InvalidJsonResponse) {
    HttpServer server(200, "not json");
    Recorder recorder;
    auto client = make_client(recorder);

    APIRequest request;
    request.endpoint = server.url();
    EXPECT_THROW(client->start_bot(request), StartBotError);
}

TEST(StartBot, Unreachable) {
    Recorder recorder;
    auto client = make_client(recorder);

    APIRequest request;
    // Nothing listens on port 1.
    request.endpoint = "http://127.0.0.1:1/start";
    try {
        client->start_bot(request);
        FAIL() << "start_bot() didn't throw";
    } catch (const StartBotError& e) {
        EXPECT_EQ(e.status(), 0);
    }
    EXPECT_EQ(client->state(), TransportState::Error);
}

TEST(StartBot, TimesOut) {
    HttpServer server(200, "{}", false);
    Recorder recorder;
    auto client = make_client(recorder);

    APIRequest request;
    request.endpoint = server.url();
    request.timeout = std::chrono::milliseconds(200);
    auto start = std::chrono::steady_clock::now();
    EXPECT_THROW(client->start_bot(request), StartBotError);
    EXPECT_LT(
            std::chrono::steady_clock::now() - start, std::chrono::seconds(5)
    );
}

TEST(StartBot, DisconnectCancels) {
    HttpServer server(200, "{}", false);
    Recorder recorder;
    auto client = make_client(recorder);

    std::thread disconnector([&] {
        // Wait until the request reached the server.
        server.request();
        client->disconnect();
    });

    APIRequest request;
    request.endpoint = server.url();
    try {
        client->start_bot(request);
        FAIL() << "start_bot() didn't throw";
    } catch (const StartBotError& e) {
        FAIL() << "Unexpected StartBotError: " << e.what();
    } catch (const PipecatError&) {
    }
    disconnector.join();

    EXPECT_EQ(client->state(), TransportState::Disconnected);
}

TEST(StartBot, StartBotAndConnect) {
    HttpServer server(200, R"({"room_url": "https://example.com/room"})");
    Recorder recorder;
    FakeTransport* transport = nullptr;
    auto client = make_client(recorder, &transport);

    APIRequest request;
    request.endpoint = server.url();
    auto data = client->start_bot_and_connect(request);

    EXPECT_EQ(data.version, "2.1.0");
    EXPECT_EQ(
            transport->connect_params(),
            json({{"room_url", "https://example.com/room"}})
    );
    EXPECT_EQ(client->state(), TransportState::Ready);
}

#endif
