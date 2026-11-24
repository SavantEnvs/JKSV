// Host stand-in for the libnx umbrella header: only the types/functions the fuzzed JKSV
// sources reference. libnx itself targets the Switch (aarch64 + Horizon services) and
// cannot be built or run on Linux.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <sys/types.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t s32;
typedef int64_t s64;
typedef u32 Result;
#define R_SUCCEEDED(res) ((res) == 0)
#define R_FAILED(res) ((res) != 0)

#define FS_MAX_PATH 0x301

enum
{
    FsOpenMode_Read   = 1 << 0,
    FsOpenMode_Write  = 1 << 1,
    FsOpenMode_Append = 1 << 2,
    FsOpenMode_Create = 1 << 8,
};

extern "C" {
// libnx nx/source/runtime/util/utf (reimplemented in mayhem/shim/libnx_utf.c)
ssize_t decode_utf8(uint32_t *out, const uint8_t *in);
ssize_t encode_utf16(uint16_t *out, uint32_t in);
ssize_t utf8_to_utf16(uint16_t *out, const uint8_t *in, size_t len);
}
