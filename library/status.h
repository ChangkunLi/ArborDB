#ifndef STATUS_H
#define STATUS_H

#include <string>
#include <iostream>

// status is a class which should be returned by almost every function call inside ArborDB

namespace mydb
{
    class Status
    {
    public:
        Status()
        {
            type_ = kOK;
            msg1 = "";
        }
        ~Status() {}
        Status(int type)
        {
            type_ = type;
        }
        Status(int type, std::string message1, std::string message2)
        {
            type_ = type;
            msg1 = message1;
            msg2 = message2;
        }

        static Status OK() { return Status(); }

        static Status Done() { return Status(kDone); }

        static Status DeleteOrder() { return Status(kDeleteRequest); }

        static Status NotFound(const std::string &message1, const std::string &message2 = "")
        {
            return Status(kNotFound, message1, message2);
        }

        static Status InvalidArgument(const std::string &message1, const std::string &message2 = "")
        {
            return Status(kInvalidArgument, message1, message2);
        }

        static Status IOError(const std::string &message1, const std::string &message2 = "")
        {
            return Status(kIOError, message1, message2);
        }

        bool IsOK() const { return (type() == kOK); }
        bool IsNotFound() const { return type() == kNotFound; }
        bool IsDeleteOrder() const { return type() == kDeleteRequest; }
        bool IsInvalidArgument() const { return type() == kInvalidArgument; }
        bool IsIOError() const { return type() == kIOError; }
        bool IsDone() const { return type() == kDone; }

        std::string ToString() const;

    private:
        int type_;
        std::string msg1;
        std::string msg2;

        int type() const { return type_; };

        enum ReturnType
        {
            kOK = 0,
            kNotFound = 1,
            kDeleteRequest = 2,
            kInvalidArgument = 3,
            kIOError = 4,
            kDone = 5,
        };
    };

    std::string Status::ToString() const
    {
        if (msg1 == "")
        {
            return "OK";
        }
        else
        {
            char tmp[30];
            const char *code;
            switch (type())
            {
            case kOK:
                code = "OK";
                break;
            case kNotFound:
                code = "Not found: ";
                break;
            case kDeleteRequest:
                code = "Delete request: ";
                break;
            case kInvalidArgument:
                code = "Invalid argument: ";
                break;
            case kIOError:
                code = "IO error: ";
                break;
            case kDone:
                code = "Done: ";
                break;
            default:
                snprintf(tmp, sizeof(tmp), "Unknown code (%d): ",
                         static_cast<int>(type()));
                code = tmp;
                break;
            }
            std::string ret(code);
            ret+= msg1;
            if (msg2.size() > 0)
            {
                ret+= " : ";
                ret+= msg2;
            }
            return ret;
        }
    }
}

#endif