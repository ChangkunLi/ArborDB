#ifndef CLIENT_TEST_H
#define CLIENT_TEST_H

#include <libmemcached/memcached.hpp>
#include <string>
#include <sstream>
#include <unordered_set>
#include <stdlib.h>
#include <unistd.h>
#include <random>
#include <chrono>
#include <cstring>

#include "library/hash.h"
#include "network_test/thread_pool.h"
#include "library/status.h"

namespace mydb
{
class Client {
public:
    Client(std::string server){
        memc = memcached(server.c_str(), server.size());
        memcached_behavior_set(memc, MEMCACHED_BEHAVIOR_CONNECT_TIMEOUT, 30000); 
        memcached_behavior_set(memc, MEMCACHED_BEHAVIOR_POLL_TIMEOUT, 30000); 
        memcached_behavior_set(memc, MEMCACHED_BEHAVIOR_RETRY_TIMEOUT, 100);
    }

    ~Client(){
        memcached_free(memc);
    }

    Status Put(const char* key, uint64_t sz_key, const char* val, uint64_t sz_val) {
        memcached_return_t ret = memcached_set(memc, key, sz_key, val, sz_val, time_t(0), uint32_t(0));
        if(ret != MEMCACHED_SUCCESS){
            std::string error_msg = std::string(key) + std::string(" ") + std::string(memcached_strerror(memc, ret));
            return Status::IOError(error_msg);
        }
        return Status::OK();
    }

    Status Delete(const char* key, uint64_t sz_key) {
        memcached_return_t ret = memcached_delete(memc, key, sz_key, time_t(0));
        if(ret != MEMCACHED_SUCCESS){
            std::string error_msg = std::string(key) + std::string(" ") + std::string(memcached_strerror(memc, ret));
            return Status::IOError(error_msg);
        }
        return Status::OK();
    }

    Status Get(const std::string& key, char **val_out, size_t *sz_val){
        char* buffer = new char[1024];
        memcached_return_t ret;
        uint32_t flags;
        const char* keys[1];
        keys[0] = key.c_str();
        size_t key_len[1];
        key_len[0] = key.size();

        char ret_key[1024];
        size_t ret_key_len;
        char *ret_val;
        size_t ret_val_len;

        ret = memcached_mget(memc, keys, key_len, 1);
        if(ret != MEMCACHED_SUCCESS){
            delete[] buffer;
            std::string error_msg = std::string(key) + std::string(" ") + std::string(memcached_strerror(memc, ret));
            return Status::IOError(error_msg);
        }

        if(ret_val = memcached_fetch(memc, ret_key, &ret_key_len, &ret_val_len, &flags, &ret)){
            memcpy(buffer, ret_val, ret_val_len);
            buffer[ret_val_len] = '\0';
            *val_out = buffer;
            *sz_val = ret_val_len;
            free(ret_val);
        }

        if(ret == MEMCACHED_END) return Status::NotFound("key: " + key);

        return Status::OK();
    }

private:
    memcached_st *memc;
};

class ClientTask : public Task {
public:
    ClientTask(std::string host, int num_puts, int num_dels, int num_gets):host_(host), 
                num_puts_(num_puts), num_dels_(num_dels), num_gets_(num_gets) {}

    ~ClientTask() {}

    void Run() {
        
    }

    char* CalculateVal(const std::string& key, int len) {
        uint64_t val = HashFunction(key.c_str(), key.size());
        char* ret = new char[len];
        int i;
        for(i=0; i<len/8; i++) memcpy(ret+i*8, &val, 8);
        if(len % 8) memcpy(ret+i*8, &val, (len%8));
        return ret;
    }

    bool CheckVal(const std::string& key, int len, const char* val_in) {
        char* ptr = CalculateVal(key, len);
        bool ret = true;
        if(memcmp(ptr, val_in, len) != 0) ret = false;
        delete[] ptr;
        return ret;
    }

private:
    std::string host_;
    int num_puts_;
    int num_dels_;
    int num_gets_;
    std::unordered_set<std::string> deleted_keys_;
};

} // namespace mydb


#endif  