// config::ConfigContext::load — JKSV.json and Paths.json from the SD card. The input is split at
// the first NUL: before → sdmc:/config/JKSV/JKSV.json, after → Paths.json (absent without a NUL).
// Both files are rewritten every input, so save() writing JKSV.json does not leak across inputs.
#include "config/ConfigContext.hpp"
#include "config/keys.hpp"
#include "scratch.h"

#include <cstdint>
#include <cstring>
#include <unistd.h>
#include <vector>

extern "C" int LLVMFuzzerInitialize(int *, char ***)
{
    jksv_make_scratch();
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    JksvScratch scratch{};
    const uint8_t *nul = static_cast<const uint8_t *>(std::memchr(data, 0, size));
    const size_t configSize = nul ? static_cast<size_t>(nul - data) : size;
    jksv_write_file("sdmc:/config/JKSV/JKSV.json", data, configSize);
    if (nul) { jksv_write_file("sdmc:/config/JKSV/Paths.json", nul + 1, size - configSize - 1); }
    else { unlink("sdmc:/config/JKSV/Paths.json"); }

    config::ConfigContext context{};
    if (!context.load()) { return 0; }
    volatile uint64_t sink = context.get_by_key(config::keys::ZIP_COMPRESSION_LEVEL);
    sink = sink + static_cast<uint64_t>(context.get_animation_scaling());
    sink = sink + context.get_working_directory().string().size();
    std::vector<uint64_t> blacklist{};
    context.get_blacklist(blacklist);
    char path[FS_MAX_PATH];
    for (uint64_t id : blacklist)
    {
        sink = sink + context.is_favorite(id);
        context.get_custom_path(id, path, sizeof(path));
    }
    context.save();
    return 0;
}
