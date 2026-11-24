// remote::WebDav — construction reads sdmc:/config/JKSV/webdav.json and PROPFINDs the base path;
// the server's multistatus XML (the fuzz input) is parsed by process_listing. The fake libcurl
// serves the input as the first response and empty bodies afterwards, which bounds the
// recursive PROPFIND of every <collection>.
#include "fake_curl.h"
#include "remote/WebDav.hpp"
#include "scratch.h"

#include <cstdint>
#include <cstring>

static const char WEBDAV_CONFIG[] =
    "{\"origin\":\"https://dav.example.com\",\"basepath\":\"JKSV\",\"username\":\"u\",\"password\":\"p\"}";

extern "C" int LLVMFuzzerInitialize(int *, char ***)
{
    jksv_make_scratch();
    JksvScratch scratch{};
    jksv_write_file("sdmc:/config/JKSV/webdav.json", WEBDAV_CONFIG, std::strlen(WEBDAV_CONFIG));
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    JksvScratch scratch{};
    fake_curl::reset({{207, std::string(reinterpret_cast<const char *>(data), size)}});
    remote::WebDav dav{};
    if (!dav.is_initialized()) { return 0; }
    remote::Storage::DirectoryListing listing{};
    dav.get_directory_listing(listing);
    for (remote::Item *item : listing)
    {
        if (item->is_directory())
        {
            remote::Storage::DirectoryListing sub{};
            dav.get_directory_listing_with_parent(item, sub);
        }
        dav.get_item_by_id(item->get_id());
    }
    return 0;
}
