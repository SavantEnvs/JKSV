// stringutil::sanitize_string_for_path — turns title/user names (from save data and the
// title database) into path components. First byte picks the caller's output buffer size
// (1..256), the rest is the NUL-terminated name.
#include "stringutil.hpp"

#include <cstdint>
#include <cstring>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 1) { return -1; }
    const size_t outSize = static_cast<size_t>(data[0]) + 1;
    std::vector<char> in(data + 1, data + size);
    in.push_back('\0');
    char *out = new char[outSize]();
    stringutil::sanitize_string_for_path(in.data(), out, outSize);
    delete[] out;

    std::string s(in.data());
    stringutil::strip_character(' ', s);
    stringutil::replace_in_string(s, "..", "_");
    return 0;
}
