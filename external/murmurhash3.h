#ifndef _MURMURHASH3_H_
#define _MURMURHASH3_H_

//-----------------------------------------------------------------------------
// Platform-specific functions and macros

// Microsoft Visual Studio

#if defined(_MSC_VER)

typedef unsigned char uint8_t;
typedef unsigned long uint32_t;
typedef unsigned __int64 uint64_t;

// Other compilers

#else // defined(_MSC_VER)

#include <stdint.h>

#endif // !defined(_MSC_VER)

//-----------------------------------------------------------------------------

namespace mydb
{
    void MurmurHash3_x86_128(const void *key, int len, uint32_t seed, void *out);

    uint64_t Murmurhash(const char* data, uint32_t len) {
        static char hash[16];
        static uint64_t ret;
        MurmurHash3_x86_128(data, len, 1997, hash);
        memcpy(&ret, hash, 8);
        return ret;
    }
} // namespace mydb

//-----------------------------------------------------------------------------

#endif // _MURMURHASH3_H_