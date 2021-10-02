#ifndef ARBORDB_H
#define ARBORDB_H

#include "library/byte_array.h"
#include "library/status.h"

namespace mydb
{
    class ArborDB
    {
    public:
        virtual ~ArborDB() {}
        virtual Status Get(ByteArray &key, ByteArray *value_out) = 0;

        virtual Status Get(ByteArray &key, std::string *value_out)
        {
            ByteArray value;
            Status s = Get(key, &value);
            if (!s.IsOK())
                return s;
            *value_out = value.ToString();
            return s;
        }

        virtual Status Get(const std::string &key, ByteArray *value_out)
        {
            // when we try to query some key, the getter thread will wait for the response,
            // so the key is in memory during the while get call, that's why we can only use 
            // a reference byte array
            ByteArray byte_array_key = ByteArray::NewReferenceByteArray(key.c_str(), key.size());
            Status s = Get(byte_array_key, value_out);
            return s;
        }

        virtual Status Get(const std::string &key, std::string *value_out)
        {
            ByteArray byte_array_key = ByteArray::NewReferenceByteArray(key.c_str(), key.size());
            ByteArray value;
            Status s = Get(key, &value);
            if (!s.IsOK())
                return s;
            *value_out = value.ToString();
            return s;
        }

        virtual Status Put(ByteArray &key, ByteArray &chunk) = 0;

        virtual Status Put(ByteArray &key, const std::string &chunk)
        {   
            // when we need to put, writer thread don't wait for the write to be persisted to disk,
            // instead, it caches the key and value in memory through deep copy (alloc), then writer 
            // thread discard the current key-value pair and ready for the next write request 
            ByteArray byte_array_chunk = ByteArray::NewCopyByteArray(chunk.c_str(), chunk.size());
            return Put(key, byte_array_chunk);
        }

        virtual Status Put(const std::string &key, ByteArray &chunk)
        {
            ByteArray byte_array_key = ByteArray::NewCopyByteArray(key.c_str(), key.size());
            return Put(byte_array_key, chunk);
        }

        virtual Status Put(const std::string &key, const std::string &chunk)
        {
            ByteArray byte_array_key = ByteArray::NewCopyByteArray(key.c_str(), key.size());
            ByteArray byte_array_chunk = ByteArray::NewCopyByteArray(chunk.c_str(), chunk.size());
            return Put(byte_array_key, byte_array_chunk);
        }

        virtual Status Delete(ByteArray &key) = 0;
        virtual Status Open() = 0;
        virtual void Close() = 0;
        virtual void Flush() = 0;
        virtual void Compact() = 0;
    };
}

#endif