#ifndef ENCODING_H
#define ENCODING_H

#include <stdint.h>
#include <string.h>
#include <string>

namespace mydb
{

    extern void PutFixed32(std::string *dst, uint32_t value);
    extern void PutFixed64(std::string *dst, uint64_t value);
    extern void PutVarint32(std::string *dst, uint32_t value);
    extern void PutVarint64(std::string *dst, uint64_t value);

    extern int GetVarint32(char *input, uint64_t size, uint32_t *value);
    extern int GetVarint64(char *input, uint64_t size, uint64_t *value);

    extern const char *GetVarint32Ptr(const char *p, const char *limit, uint32_t *v);
    extern const char *GetVarint64Ptr(const char *p, const char *limit, uint64_t *v);


    extern int VarintLength(uint64_t v);


    extern void EncodeFixed32(char *dst, uint32_t value);
    extern void EncodeFixed64(char *dst, uint64_t value);

    extern void GetFixed32(const char *src, uint32_t *value);
    extern void GetFixed64(const char *src, uint64_t *value);


    extern char *EncodeVarint32(char *dst, uint32_t value);
    extern char *EncodeVarint64(char *dst, uint64_t value);


    inline uint32_t DecodeFixed32(const char *ptr)
    {
        // Load the raw bytes
        uint32_t result;
        memcpy(&result, ptr, sizeof(result)); // gcc optimizes this to a plain load
        return result;
    }

    inline uint64_t DecodeFixed64(const char *ptr)
    {
        // Load the raw bytes
        uint64_t result;
        memcpy(&result, ptr, sizeof(result)); // gcc optimizes this to a plain load
        return result;
    }

    // Internal routine for use by fallback path of GetVarint32Ptr
    extern const char *GetVarint32PtrFallback(const char *p,
                                              const char *limit,
                                              uint32_t *value);
    inline const char *GetVarint32Ptr(const char *p,
                                      const char *limit,
                                      uint32_t *value)
    {
        if (p < limit)
        {
            uint32_t result = *(reinterpret_cast<const unsigned char *>(p));
            if ((result & 128) == 0)
            {
                *value = result;
                return p + 1;
            }
        }
        return GetVarint32PtrFallback(p, limit, value);
    }

} // namespace mydb

#endif