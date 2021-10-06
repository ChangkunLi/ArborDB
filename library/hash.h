#ifndef _HASH_H
#define _HASH_H

#include <cstdint>
#include <cstring>

#include "external/murmurhash3.h"

namespace mydb
{
    inline uint64_t HashFunction(const char* data, uint32_t len){
        static char hash[16];
        static uint64_t ret;
        MurmurHash3_x86_128(data,len,0,hash);
        memcpy(&ret, hash, 8);
        return ret;
    }    
} // namespace mydb


#endif