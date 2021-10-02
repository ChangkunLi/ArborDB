#ifndef BYTE_ARRAY_H
#define BYTE_ARRAY_H

#include <iostream>
#include <memory>
#include <string>
#include <string.h>

// implement a byte array class to store key, value and other storage requirement
namespace mydb
{
    class ByteArrayResource
    {
    public:
        ByteArrayResource() {}
        virtual ~ByteArrayResource() {}
        virtual char *bytes() = 0;
        virtual const char *bytes_const() = 0;
        virtual uint64_t size() = 0;
    };

    class ReferenceByteArrayResource : public ByteArrayResource
    {
        friend class ByteArray;

    public:
        ReferenceByteArrayResource(const char *bytes, uint64_t size) : bytes_(bytes), size_(size) {}
        ~ReferenceByteArrayResource() {}
        char *bytes() { return const_cast<char *>(bytes_); }
        const char *bytes_const() { return bytes_; }
        uint64_t size() { return size_; }

    private:
        const char *bytes_;
        uint64_t size_;
    };

    class CopyByteArrayResource : public ByteArrayResource
    {
        friend class ByteArray;

    public:
        CopyByteArrayResource(const char *bytes, uint64_t size)
        {
            bytes_ = new char[size];
            memcpy(bytes_, bytes, size);
        }
        ~CopyByteArrayResource()
        {
            delete[] bytes_;
        }
        char *bytes() { return bytes_; }
        const char *bytes_const() { return const_cast<const char *>(bytes_); }
        uint64_t size() { return size_; }
        // in derived class, we have to override pure virtual function, otherwise the derived class also
        // becomes abstract class, and we can not instantiate an abstract class.
        // Notice that in order to use smart pointer via std::make_shared<>, the derived class has to be
        // instantiated, that's why we need to override(implement) every pure virtual function!!!

    private:
        char *bytes_;
        uint64_t size_;
    };

    class ByteArray
    {
    public:
        ByteArray() : size_(0), offset_(0) {}

        static ByteArray NewReferenceByteArray(const char *bytes, uint64_t size)
        {
            ByteArray ret;
            ret.resource_ = std::make_shared<ReferenceByteArrayResource>(bytes, size);
            ret.size_ = size;
            return ret;
        }

        static ByteArray NewCopyByteArray(const char *bytes, uint64_t size)
        {
            ByteArray ret;
            ret.resource_ = std::make_shared<CopyByteArrayResource>(bytes, size);
            ret.size_ = size;
            return ret;
        }

        char *bytes() { return resource_->bytes() + offset_; }
        const char *bytes_const() const { return resource_->bytes_const() + offset_; }
        virtual uint64_t size() { return size_; }
        void set_offset(uint64_t set) { offset_ = set; }
        void inc_offset(uint64_t inc) { offset_ += inc; }

        std::string ToString()
        {
            return std::string(bytes(), size());
        }

        bool operator==(ByteArray &right)
        {
            return (size() == right.size() && memcmp(bytes_const(), right.bytes_const(), size()) == 0);
        }

    private:
        std::shared_ptr<ByteArrayResource> resource_; // abstract class can have pointer
        uint64_t size_;
        uint64_t offset_;
    };
}

#endif