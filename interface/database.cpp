#include "interface/database.h"

namespace mydb
{
    void Database::Flush() { c_buf->Flush(); }

    void Database::Compact() {
        Flush();
        d_eng->FlushFile();
        d_eng->Compact();
    }

    Status Database::Get(ByteArray &key, ByteArray *value_out) {
        if(is_closed) return Status::IOError("Database is closed");
        Status ret = c_buf->Get(key,value_out);
        if(ret.IsDeleteOrder()){
            return Status::NotFound("Record was deleted");
        }    
        else if(ret.IsNotFound()){
            ret = d_eng->Get(key,value_out);
            if(ret.IsNotFound()) return ret;
        }  

        return ret;  
    } 

    Status Database::Delete(ByteArray& key){
        return c_buf->Delete(key);
    }

    Status Database::Put(ByteArray &key, ByteArray &chunk){
        if(is_closed) return Status::IOError("Database is closed");
        return c_buf->Put(key,chunk);
    }

} // namespace mydb
