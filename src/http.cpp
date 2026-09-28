//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#include "http.h"

#include "pipecat/errors.h"

#include <curl/curl.h>

#include <cctype>
#include <memory>
#include <mutex>

namespace pipecat {

namespace {

struct CurlDeleter {
    void operator()(CURL* curl) const { curl_easy_cleanup(curl); }
};

struct CurlListDeleter {
    void operator()(curl_slist* list) const { curl_slist_free_all(list); }
};

using CurlPtr = std::unique_ptr<CURL, CurlDeleter>;
using CurlListPtr = std::unique_ptr<curl_slist, CurlListDeleter>;

void append_header(CurlListPtr& list, const std::string& header) {
    curl_slist* appended = curl_slist_append(list.get(), header.c_str());
    if (appended == nullptr) {
        throw StartBotError("Unable to allocate request headers");
    }
    list.release();
    list.reset(appended);
}

bool is_content_type(const std::string& name) {
    const std::string content_type = "content-type";
    if (name.size() != content_type.size()) {
        return false;
    }
    for (size_t i = 0; i < name.size(); i++) {
        if (std::tolower(static_cast<unsigned char>(name[i])) !=
            content_type[i]) {
            return false;
        }
    }
    return true;
}

size_t write_callback(char* data, size_t size, size_t count, void* user_data) {
    static_cast<std::string*>(user_data)->append(data, size * count);
    return size * count;
}

int progress_callback(
        void* user_data,
        curl_off_t /* download_total */,
        curl_off_t /* downloaded */,
        curl_off_t /* upload_total */,
        curl_off_t /* uploaded */
) {
    // Returning non-zero aborts the transfer.
    const auto& cancelled =
            *static_cast<const std::function<bool()>*>(user_data);
    return cancelled() ? 1 : 0;
}

}  // namespace

HttpResponse
http_post(const APIRequest& request, const std::function<bool()>& cancelled) {
    static std::once_flag curl_initialized;
    std::call_once(curl_initialized, [] {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    });

    CurlPtr curl(curl_easy_init());
    if (!curl) {
        throw StartBotError("Unable to initialize libcurl");
    }

    CurlListPtr headers;
    bool has_content_type = false;
    for (const auto& [name, value]: request.headers) {
        append_header(headers, name + ": " + value);
        has_content_type = has_content_type || is_content_type(name);
    }
    if (!has_content_type) {
        append_header(headers, "Content-Type: application/json");
    }

    std::string body =
            request.request_data.is_null() ? "" : request.request_data.dump();

    HttpResponse response;

    curl_easy_setopt(curl.get(), CURLOPT_URL, request.endpoint.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers.get());
    curl_easy_setopt(curl.get(), CURLOPT_POST, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(
            curl.get(), CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size())
    );
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(curl.get(), CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl.get(), CURLOPT_XFERINFOFUNCTION, progress_callback);
    curl_easy_setopt(curl.get(), CURLOPT_XFERINFODATA, &cancelled);
    // Signals aren't safe with multiple threads.
    curl_easy_setopt(curl.get(), CURLOPT_NOSIGNAL, 1L);
    if (request.timeout.count() > 0) {
        curl_easy_setopt(
                curl.get(),
                CURLOPT_TIMEOUT_MS,
                static_cast<long>(request.timeout.count())
        );
    }

    CURLcode result = curl_easy_perform(curl.get());
    if (result == CURLE_ABORTED_BY_CALLBACK) {
        throw PipecatError("start_bot() was cancelled by disconnect()");
    }
    if (result != CURLE_OK) {
        throw StartBotError(
                "Unable to reach " + request.endpoint + ": " +
                curl_easy_strerror(result)
        );
    }

    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &response.status);

    return response;
}

}  // namespace pipecat
