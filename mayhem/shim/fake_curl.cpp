#include "fake_curl.h"

#define CURL_DISABLE_TYPECHECK
#include <curl/curl.h>
#include <cstdio>

#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <deque>

namespace
{
    struct FakeHandle
    {
        curl_write_callback write{};
        void *writeData{};
        curl_write_callback header{};
        void *headerData{};
        long lastCode{0};
    };
    std::deque<fake_curl::Response> g_queue{};
    size_t g_performed{};
} // namespace

void fake_curl::reset(std::vector<Response> responses)
{
    g_queue.assign(responses.begin(), responses.end());
    g_performed = 0;
}
size_t fake_curl::performed() { return g_performed; }

extern "C" {
CURLcode curl_global_init(long) { return CURLE_OK; }
void curl_global_cleanup(void) {}
CURL *curl_easy_init(void) { return reinterpret_cast<CURL *>(new FakeHandle{}); }
void curl_easy_cleanup(CURL *curl) { delete reinterpret_cast<FakeHandle *>(curl); }
void curl_easy_reset(CURL *curl)
{
    FakeHandle *h = reinterpret_cast<FakeHandle *>(curl);
    *h            = FakeHandle{};
}
CURLcode curl_easy_setopt(CURL *curl, CURLoption option, ...)
{
    FakeHandle *h = reinterpret_cast<FakeHandle *>(curl);
    va_list args;
    va_start(args, option);
    switch (option)
    {
        case CURLOPT_WRITEFUNCTION: h->write = va_arg(args, curl_write_callback); break;
        case CURLOPT_WRITEDATA: h->writeData = va_arg(args, void *); break;
        case CURLOPT_HEADERFUNCTION: h->header = va_arg(args, curl_write_callback); break;
        case CURLOPT_HEADERDATA: h->headerData = va_arg(args, void *); break;
        default: break;
    }
    va_end(args);
    return CURLE_OK;
}
CURLcode curl_easy_perform(CURL *curl)
{
    FakeHandle *h = reinterpret_cast<FakeHandle *>(curl);
    g_performed++;
    fake_curl::Response r{};
    if (!g_queue.empty())
    {
        r = std::move(g_queue.front());
        g_queue.pop_front();
    }
    else { r.body.clear(); }
    h->lastCode = r.code;
    if (h->write && !r.body.empty())
    {
        // Copy into an exact-size heap buffer so ASan sees reads past the delivered chunk.
        char *chunk = static_cast<char *>(std::malloc(r.body.size()));
        std::memcpy(chunk, r.body.data(), r.body.size());
        const size_t taken = h->write(chunk, 1, r.body.size(), h->writeData);
        std::free(chunk);
        if (taken != r.body.size()) { return CURLE_WRITE_ERROR; }
    }
    return CURLE_OK;
}
CURLcode curl_easy_getinfo(CURL *curl, CURLINFO info, ...)
{
    FakeHandle *h = reinterpret_cast<FakeHandle *>(curl);
    va_list args;
    va_start(args, info);
    if (info == CURLINFO_RESPONSE_CODE) { *va_arg(args, long *) = h->lastCode; }
    va_end(args);
    return CURLE_OK;
}
static bool is_unreserved(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' ||
           c == '~';
}
char *curl_easy_escape(CURL *, const char *string, int length)
{
    const size_t n = length ? static_cast<size_t>(length) : std::strlen(string);
    char *out      = static_cast<char *>(std::malloc(n * 3 + 1));
    size_t o       = 0;
    for (size_t i = 0; i < n; i++)
    {
        const unsigned char c = string[i];
        if (is_unreserved(c)) { out[o++] = c; }
        else { o += std::snprintf(out + o, 4, "%%%02X", c); }
    }
    out[o] = '\0';
    return out;
}
static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
char *curl_easy_unescape(CURL *, const char *string, int length, int *outlength)
{
    const size_t n = length ? static_cast<size_t>(length) : std::strlen(string);
    char *out      = static_cast<char *>(std::malloc(n + 1));
    size_t o       = 0;
    for (size_t i = 0; i < n; i++)
    {
        if (string[i] == '%' && i + 2 < n && hexval(string[i + 1]) >= 0 &&
            hexval(string[i + 2]) >= 0)
        {
            out[o++] = static_cast<char>(hexval(string[i + 1]) * 16 + hexval(string[i + 2]));
            i += 2;
        }
        else { out[o++] = string[i]; }
    }
    out[o] = '\0';
    if (outlength) { *outlength = static_cast<int>(o); }
    return out;
}
void curl_free(void *p) { std::free(p); }
struct curl_slist *curl_slist_append(struct curl_slist *list, const char *data)
{
    curl_slist *node = static_cast<curl_slist *>(std::calloc(1, sizeof(curl_slist)));
    node->data       = strdup(data);
    if (!list) { return node; }
    curl_slist *last = list;
    while (last->next) { last = last->next; }
    last->next = node;
    return list;
}
void curl_slist_free_all(struct curl_slist *list)
{
    while (list)
    {
        curl_slist *next = list->next;
        std::free(list->data);
        std::free(list);
        list = next;
    }
}
}
