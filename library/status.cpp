#include "library/status.h"

namespace mydb{
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