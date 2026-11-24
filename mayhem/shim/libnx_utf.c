// Host reimplementation of the libnx UTF helpers JKSV calls (libnx nx/source/runtime/util/utf:
// decode_utf8.c, encode_utf16.c, utf8_to_utf16.c, ISC). Same contracts: decode_utf8 returns the
// number of bytes consumed or -1 on an invalid/overlong/surrogate sequence; utf8_to_utf16 converts
// up to the NUL, writes at most `len` units (no terminator) and returns the total units needed.
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <sys/types.h>

ssize_t decode_utf8(uint32_t *out, const uint8_t *in)
{
    uint8_t code1, code2, code3, code4;
    code1 = *in++;
    if (code1 < 0x80) { *out = code1; return 1; }
    else if (code1 < 0xC2) return -1;
    else if (code1 < 0xE0)
    {
        code2 = *in++;
        if ((code2 & 0xC0) != 0x80) return -1;
        *out = (code1 << 6) + code2 - 0x3080;
        return 2;
    }
    else if (code1 < 0xF0)
    {
        code2 = *in++;
        if ((code2 & 0xC0) != 0x80) return -1;
        if (code1 == 0xE0 && code2 < 0xA0) return -1;
        code3 = *in++;
        if ((code3 & 0xC0) != 0x80) return -1;
        *out = (code1 << 12) + (code2 << 6) + code3 - 0xE2080;
        if (*out >= 0xD800 && *out <= 0xDFFF) return -1;
        return 3;
    }
    else if (code1 < 0xF5)
    {
        code2 = *in++;
        if ((code2 & 0xC0) != 0x80) return -1;
        if (code1 == 0xF0 && code2 < 0x90) return -1;
        if (code1 == 0xF4 && code2 >= 0x90) return -1;
        code3 = *in++;
        if ((code3 & 0xC0) != 0x80) return -1;
        code4 = *in++;
        if ((code4 & 0xC0) != 0x80) return -1;
        *out = (code1 << 18) + (code2 << 12) + (code3 << 6) + code4 - 0x3C82080;
        return 4;
    }
    return -1;
}

ssize_t encode_utf16(uint16_t *out, uint32_t in)
{
    if (in < 0x10000)
    {
        if (out != NULL) *out++ = in;
        return 1;
    }
    else if (in < 0x110000)
    {
        if (out != NULL)
        {
            *out++ = (in >> 10) + 0xD7C0;
            *out++ = (in & 0x3FF) + 0xDC00;
        }
        return 2;
    }
    return -1;
}

ssize_t utf8_to_utf16(uint16_t *out, const uint8_t *in, size_t len)
{
    ssize_t rc = 0;
    ssize_t units;
    uint32_t code;
    uint16_t encoded[2] = {0};
    do {
        units = decode_utf8(&code, in);
        if (units == -1) return -1;
        if (code > 0)
        {
            in += units;
            units = encode_utf16(encoded, code);
            if (units == -1) return -1;
            if (out != NULL)
            {
                if ((size_t)(rc + units) <= len)
                {
                    *out++ = encoded[0];
                    if (units > 1) *out++ = encoded[1];
                }
            }
            if (SSIZE_MAX - units >= rc) rc += units;
            else return -1;
        }
    } while (code > 0);
    return rc;
}
