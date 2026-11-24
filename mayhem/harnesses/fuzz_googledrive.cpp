// remote::GoogleDrive — client_secret.json from the SD card plus every Google OAuth2/Drive v3 JSON
// response JKSV parses. The input is NUL-separated: segment 0 → sdmc:/config/JKSV/client_secret.json,
// segments 1.. → the HTTP bodies the fake libcurl serves in order (token refresh, root id, listing
// pages, then create_directory, device-code sign-in and its poll). Bodies run out → empty responses.
#include "fake_curl.h"
#include "remote/GoogleDrive.hpp"
#include "scratch.h"

#include <cstdint>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

extern "C" int LLVMFuzzerInitialize(int *, char ***)
{
    jksv_make_scratch();
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    JksvScratch scratch{};
    std::vector<std::string> segments{};
    const char *p   = reinterpret_cast<const char *>(data);
    const char *end = p + size;
    while (true)
    {
        const char *nul = static_cast<const char *>(std::memchr(p, 0, end - p));
        segments.emplace_back(p, nul ? nul : end);
        if (!nul) { break; }
        p = nul + 1;
    }
    jksv_write_file("sdmc:/config/JKSV/client_secret.json", segments[0].data(), segments[0].size());
    std::vector<fake_curl::Response> responses{};
    for (size_t i = 1; i < segments.size(); i++) { responses.push_back({200, segments[i]}); }
    fake_curl::reset(std::move(responses));

    remote::GoogleDrive drive{};
    if (drive.is_initialized())
    {
        remote::Storage::DirectoryListing listing{};
        drive.get_directory_listing(listing);
        drive.create_directory("JKSV");
    }
    std::string message{}, code{};
    std::time_t expiration{};
    int wait{};
    if (drive.get_sign_in_data(message, code, expiration, wait)) { drive.poll_sign_in(code); }
    return 0;
}
