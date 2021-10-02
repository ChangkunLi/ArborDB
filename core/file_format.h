#ifndef FILEFORMAT_H
#define FILEFORMAT_H

#include <unistd.h>

#include "library/status.h"
#include "external/encoding.h"

namespace mydb{

class RecordHeader{
public:
    RecordHeader() {IsDelete_ = 0;}
    uint32_t IsDelete_;
    uint64_t size_key;
    uint64_t size_val;
    uint64_t hashed_key;

    void SetDelete() { IsDelete_ = 1; }

    void SetPut() { IsDelete_ = 0; }

    bool IsDelete() { return IsDelete_ == 1; }

    bool IsPut() { return IsDelete_ == 0; }

    static Status Decoder(const char* buf_in, RecordHeader* ret) {
        char* buf = const_cast<char*>(buf_in);
        char* p = buf;

        GetFixed32(p, &(ret->IsDelete_));
        p+= 4;

        GetFixed64(p, &(ret->size_key));
        p+= 8;

        GetFixed64(p, &(ret->size_val));
        p+= 8;

        GetFixed64(p, &(ret->hashed_key));
        p+= 8;

        return Status::OK();
    }

    static void Encoder(const RecordHeader* in, char* buf) {
        EncodeFixed32(buf, in->IsDelete_);
        buf+= 4;
        EncodeFixed64(buf, in->size_key);
        buf+= 8;
        EncodeFixed64(buf, in->size_val);
        buf+= 8;
        EncodeFixed64(buf, in->hashed_key);
    }

    static uint32_t GetSize() { return 28; } 
};

class FileHeader{
public:
    FileHeader() { IsCompacted_ = 0; }

    uint32_t IsCompacted_;
    uint64_t timestamp;

    bool IsCompacted() { return IsCompacted_  == 1; }

    void SetCompacted() { IsCompacted_ = 1; }

    void SetUnCompacted() { IsCompacted_ = 0; }

    static Status Decoder(const char* buf_in, FileHeader* ret) {
        GetFixed32(buf_in, &(ret->IsCompacted_));
        GetFixed64(buf_in+4, &(ret->timestamp));
        return Status::OK();
    }

    static void Encoder(const FileHeader* in, char* buf) {
        EncodeFixed32(buf, in->IsCompacted_);
        EncodeFixed64(buf+4, in->timestamp);
    }

    static uint32_t GetSize() { return 12; }
};

class FileFooter{
public:
    FileFooter() { IsCompacted_ = 0; }

    uint32_t IsCompacted_;
    uint64_t offset_InternalMap;
    uint64_t num_records;
    uint64_t magic_number;

    bool IsCompacted() { return IsCompacted_  == 1; }

    void SetCompacted() { IsCompacted_ = 1; }

    void SetUnCompacted() { IsCompacted_ = 0; }

    static Status Decoder(const char* buf_in, FileFooter* ret) {
        GetFixed32(buf_in, &(ret->IsCompacted_));
        GetFixed64(buf_in+4, &(ret->offset_InternalMap));
        GetFixed64(buf_in+12, &(ret->num_records));
        GetFixed64(buf_in+20, &(ret->magic_number));
        return Status::OK();
    }

    static void Encoder(const FileFooter* in, char* buf) {
        EncodeFixed32(buf, in->IsCompacted_);
        EncodeFixed64(buf+4, in->offset_InternalMap);
        EncodeFixed64(buf+12, in->num_records);
        EncodeFixed64(buf+20, in->magic_number);
    }

    static uint32_t GetSize() { return 28; }
};

class InternalMapLine{
public:
    uint64_t hashed_key;
    uint64_t offset_record;

    static Status Decorder(const char* buf_in, InternalMapLine* ret) {
        GetFixed64(buf_in, &(ret->hashed_key));
        GetFixed64(buf_in+8, &(ret->offset_record));
        return Status::OK();
    }

    static void Encoder(const InternalMapLine* in, char* buf) {
        EncodeFixed64(buf, in->hashed_key);
        EncodeFixed64(buf+8, in->offset_record);
    }

    static uint32_t GetSize() { return 16; }
};

} // namespace mydb

#endif