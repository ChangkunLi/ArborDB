#ifndef DATABASE_H
#define DATABASE_H

#include <sys/stat.h>
#include <thread>
#include <string>
#include <memory>
#include <sys/file.h>
#include <cstdint>

#include "interface/arbordb.h"
#include "cache/cache_buffer.h"
#include "core/engine.h"
#include "library/event_manager.h"
#include "library/fileutil.h"

namespace mydb
{
    class Database : public ArborDB{
        public:
            Database(const std::string& name_in) {
                name = getName(name_in);
                is_closed  = true;
            }
            
            virtual ~Database() { Close(); }

            std::string getName(const std::string& name_in) {
                if(name_in.size()>0 && name_in[0] == '/') return name_in; // it is already full path
                else if(name_in[0]=='.' && name_in[1]=='/'){
                    return FileUtil::getcwd_str() + "/" + name_in.substr(2);
                }
                else{
                    return FileUtil::getcwd_str() + "/" + name_in;
                }
            }

            virtual void Close() {
                std::unique_lock<std::mutex> lock(mutex_close);
                if(is_closed) return;
                is_closed = true;
                c_buf->Close();
                d_eng->Close();
                delete c_buf;
                delete d_eng;
                delete evm;
            }         

            virtual void Flush();

            virtual void Compact();

            virtual Status Open() {
                struct stat sb;
                bool is_exist = stat(name.c_str(), &sb)==0;
                if(is_exist && S_ISREG(sb.st_mode)){
                    return Status::IOError("Exists regular file with the same name, considering change file name or use another database name");
                }

                if(!is_exist && mkdir(name.c_str(), 0755)<0){
                    return Status::IOError("Failed to create directory for databse", strerror(errno));
                }

                std::unique_lock<std::mutex> lock(mutex_close);
                if(!is_closed) return Status::IOError("Database has already been open");

                evm = new EventManager();
                c_buf = new CacheBuffer(32*1024*1024,evm);
                d_eng = new DiskEngine(evm, name);
                is_closed = false;
                return Status::OK();
            }

            virtual Status Get(ByteArray &key, ByteArray *value_out);

            virtual Status Put(ByteArray &key, ByteArray &chunk);

            virtual Status Delete(ByteArray &key);

            // some overload

            virtual Status Get(ByteArray &key, std::string* value_out){
                return ArborDB::Get(key, value_out);
            }

            virtual Status Get(const std::string &key, ByteArray *value_out){
                return ArborDB::Get(key, value_out);
            }

            virtual Status Get(const std::string &key, std::string *value_out){
                return ArborDB::Get(key, value_out);
            }

            virtual Status Put(ByteArray &key, const std::string &chunk){
                return ArborDB::Put(key, chunk);
            }

            virtual Status Put(const std::string &key, ByteArray &chunk){
                return ArborDB::Put(key, chunk);
            }

            virtual Status Put(const std::string &key, const std::string &chunk){
                return ArborDB::Put(key, chunk);
            }

        private:
            std::string name;
            CacheBuffer* c_buf; 
            DiskEngine* d_eng; 
            EventManager* evm;
            bool is_closed;
            std::mutex mutex_close;
    };

} // namespace mydb


#endif