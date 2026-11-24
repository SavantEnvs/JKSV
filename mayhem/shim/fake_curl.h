// Fake libcurl for the remote-storage harnesses: no network. Each curl_easy_perform that has a
// CURLOPT_WRITEFUNCTION hands it the next queued response body (empty once the queue is drained)
// and reports the queued HTTP status.
#pragma once
#include <string>
#include <vector>

namespace fake_curl
{
    struct Response
    {
        long code{200};
        std::string body{};
    };
    void reset(std::vector<Response> responses);
    size_t performed();
} // namespace fake_curl
