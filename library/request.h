#ifndef REQUEST_H
#define REQUEST_H

#include <set>
#include <thread>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "library/status.h"
#include "library/byte_array.h"

namespace mydb
{
    enum class TypeRequest {Put, Delete};

    struct Request{
        std::thread::id thread_id;
        TypeRequest type;
        ByteArray key;
        ByteArray val;
        uint64_t size_val;
    };
}

#endif