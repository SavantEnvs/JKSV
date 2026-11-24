// Known-answer tests for the JKSV code the fuzz targets reach, built with normal flags by
// mayhem/build.sh and run by mayhem/test.sh. JKSV ships no test suite of its own (it is a Switch
// app), so these pin the observable behaviour of each parser: sanitised path names, UTF-16
// dictionary words, URL building, header parsing, the WebDav PROPFIND listing, config loading and
// backup-ZIP reading. Prints one "PASS <name>" / "FAIL <name>" line per check.
#include "config/ConfigContext.hpp"
#include "config/keys.hpp"
#include "curl/curl.hpp"
#include "fake_curl.h"
#include "fs/MiniUnzip.hpp"
#include "keyboard/Dictionary.hpp"
#include "remote/URL.hpp"
#include "remote/WebDav.hpp"
#include "scratch.h"
#include "stringutil.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

static int g_failed = 0;
static void check(const char *name, bool ok)
{
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok) { g_failed++; }
}

static std::string sanitize(const char *in, size_t outSize, bool *ok)
{
    std::vector<char> out(outSize, '\0');
    *ok = stringutil::sanitize_string_for_path(in, out.data(), outSize);
    return std::string(out.data());
}

static std::string read_file(const std::string &path)
{
    std::ifstream f(path, std::ios::binary);
    std::stringstream s;
    s << f.rdbuf();
    return s.str();
}

int main(int argc, char **argv)
{
    const std::string seeds = argc > 1 ? argv[1] : "/mayhem/mayhem";
    bool ok = false;

    // --- stringutil ---
    std::string s = sanitize("Zelda: Breath of the Wild\xe2\x84\xa2", 0x100, &ok);
    check("sanitize_forbidden_chars", ok && s == "Zelda Breath of the Wild");
    s = sanitize("Pok\xc3\xa9mon \xc3\x86ther \xe2\x85\xa2...", 0x100, &ok);
    check("sanitize_replacements_and_trim", ok && s == "Pokemon AEther III");
    s = sanitize("bad\xff", 0x100, &ok);
    check("sanitize_rejects_invalid_utf8", !ok);
    s = sanitize("0123456789", 5, &ok);
    check("sanitize_rejects_small_buffer", !ok);
    std::string r = "a..b..c";
    stringutil::replace_in_string(r, "..", "/");
    check("replace_in_string", r == "a/b/c");
    r = "\r\nhe llo\n";
    stringutil::strip_character('\n', r);
    stringutil::strip_character('\r', r);
    check("strip_character", r == "he llo");
    check("get_formatted_string", stringutil::get_formatted_string("%016llX", 0x01006F8002326000ULL) == "01006F8002326000");

    // --- keyboard::Dictionary ---
    keyboard::Dictionary dict{"Pok\xc3\xa9", "\xf0\x9f\x8e\xae"};
    const keyboard::Dictionary::Word *w = dict.get_words();
    check("dictionary_count", dict.get_count() == 2);
    check("dictionary_bmp", w[0].word[0] == u'P' && w[0].word[3] == 0xE9 && w[0].word[4] == 0 && w[0].predict[3] == 0xE9);
    check("dictionary_surrogates", w[1].word[0] == 0xD83C && w[1].word[1] == 0xDFAE && w[1].word[2] == 0);

    // --- remote::URL / curl helpers ---
    remote::URL url{"https://example.com"};
    url.append_path("drive/v3").append_path("files").append_parameter("q", "x").append_parameter("pageToken", "t");
    check("url_build", std::string(url.get()) == "https://example.com/drive/v3/files?q=x&pageToken=t");
    curl::HeaderArray headers{"HTTP/1.1 200 OK\r\n", "Location:   https://up.example.com/1\r\n"};
    std::string value{};
    check("curl_get_header_value", curl::get_header_value(headers, "Location", value) && value == "https://up.example.com/1");
    check("curl_get_header_missing", !curl::get_header_value(headers, "ETag", value));

    jksv_make_scratch();
    if (chdir(jksv_scratch_dir().c_str()) != 0) { return 2; }

    // --- remote::WebDav ---
    const char *davConfig = "{\"origin\":\"https://dav.example.com\",\"basepath\":\"JKSV\"}";
    jksv_write_file("sdmc:/config/JKSV/webdav.json", davConfig, std::strlen(davConfig));
    fake_curl::reset({{207, read_file(seeds + "/fuzz_webdav/testsuite/propfind.xml")}});
    {
        remote::WebDav dav{};
        check("webdav_initialized", dav.is_initialized());
        remote::Storage::DirectoryListing listing{};
        dav.get_directory_listing(listing);
        check("webdav_listing_count", listing.size() == 2);
        remote::Item *dir  = dav.get_directory_by_name("Zelda BotW");
        remote::Item *file = dav.get_file_by_name("save 1.zip");
        check("webdav_directory", dir && dir->is_directory() && dir->get_id() == "/JKSV/Zelda%20BotW/");
        check("webdav_file", file && !file->is_directory() && file->get_size() == 12345 && file->get_id() == "/JKSV/save%201.zip");
        check("webdav_recursed_into_collection", fake_curl::performed() == 2);
    }
    fake_curl::reset({{207, "<not-xml"}});
    {
        remote::WebDav dav{};
        check("webdav_rejects_bad_xml", !dav.is_initialized());
    }

    // --- config::ConfigContext ---
    {
        const std::string both = read_file(seeds + "/fuzz_config/testsuite/config.json");
        const size_t nul       = both.find('\0');
        jksv_write_file("sdmc:/config/JKSV/JKSV.json", both.data(), nul);
        jksv_write_file("sdmc:/config/JKSV/Paths.json", both.data() + nul + 1, both.size() - nul - 1);
        config::ConfigContext ctx{};
        check("config_defaults", ctx.get_by_key(config::keys::ZIP_COMPRESSION_LEVEL) == 6);
        check("config_load", ctx.load());
        check("config_value", ctx.get_by_key(config::keys::ZIP_COMPRESSION_LEVEL) == 9 &&
                                  ctx.get_by_key(config::keys::AUTO_UPLOAD) == 1);
        check("config_working_directory", ctx.get_working_directory().string() == "sdmc:/JKSV2");
        check("config_scaling", ctx.get_animation_scaling() == 1.5);
        check("config_favorites", ctx.is_favorite(0x0100000000010000ULL) && !ctx.is_favorite(0x01006F8002326000ULL));
        check("config_blacklist", ctx.is_blacklisted(0x01006F8002326000ULL) && ctx.is_blacklisted(0x0100ABCDEF000000ULL));
        char path[FS_MAX_PATH] = {0};
        ctx.get_custom_path(0x01006F8002326000ULL, path, sizeof(path));
        check("config_custom_path", std::string(path) == "sdmc:/custom/path" && ctx.has_custom_path(0x01006F8002326000ULL));
        ctx.save();
        config::ConfigContext again{};
        check("config_save_roundtrip", again.load() && again.get_by_key(config::keys::ZIP_COMPRESSION_LEVEL) == 9 &&
                                           again.is_blacklisted(0x0100ABCDEF000000ULL));
    }

    // --- fs::MiniUnzip ---
    {
        fs::MiniUnzip unzip{fslib::Path{seeds + "/fuzz_miniunzip/testsuite/backup.zip"}};
        check("unzip_open", unzip.is_open());
        char buf[64] = {0};
        check("unzip_locate_meta", unzip.locate_file(".nx_save_meta.bin") && unzip.read(buf, sizeof(buf)) == 4 &&
                                       std::memcmp(buf, "META", 4) == 0);
        std::vector<std::string> names{};
        std::string content{};
        unzip.reset();
        do {
            names.emplace_back(std::string(unzip.get_filename()) + (unzip.is_directory() ? "|d" : "|f"));
            if (std::string(unzip.get_filename()) == "save/dir/file.bin")
            {
                std::memset(buf, 0, sizeof(buf));
                const ssize_t n = unzip.read(buf, sizeof(buf));
                content.assign(buf, n > 0 ? n : 0);
            }
        } while (unzip.next_file());
        check("unzip_entries", names == std::vector<std::string>{".nx_save_meta.bin|f", "save/|d", "save/dir/|d", "save/dir/file.bin|f"});
        check("unzip_inflate", content == "hello save");
        fs::MiniUnzip bad{fslib::Path{"sdmc:/config/JKSV/Paths.json"}};
        check("unzip_rejects_non_zip", !bad.is_open());
    }

    std::printf("SUMMARY failed=%d\n", g_failed);
    return g_failed ? 1 : 0;
}
