// fs::MiniUnzip — reading a backup ZIP from the SD card: locate the .nx_save_meta.bin blob the
// import path reads first, then walk every entry (name, directory check, contents).
#include "fs/MiniUnzip.hpp"
#include "scratch.h"

#include <cstdint>

extern "C" int LLVMFuzzerInitialize(int *, char ***)
{
    jksv_make_scratch();
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    JksvScratch scratch{};
    jksv_write_file("input.zip", data, size);
    fs::MiniUnzip unzip{fslib::Path{"input.zip"}};
    if (!unzip.is_open()) { return 0; }
    static char buffer[0x4000];
    if (unzip.locate_file(".nx_save_meta.bin")) { unzip.read(buffer, sizeof(buffer)); }
    if (!unzip.reset()) { return 0; }
    int entries = 0;
    do {
        volatile bool dir = unzip.is_directory();
        (void)dir;
        volatile size_t sink = unzip.get_uncompressed_size() + unzip.get_compressed_size() + unzip.get_filename()[0];
        (void)sink;
        while (unzip.read(buffer, sizeof(buffer)) > 0) {}
        unzip.close_current_file();
    } while (++entries < 256 && unzip.next_file());
    return 0;
}
