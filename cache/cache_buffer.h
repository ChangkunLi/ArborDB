#ifndef CACHE_BUFFER_H
#define CACHE_BUFFER_H

#include <thread>
#include <map>
#include <string>
#include <vector>
#include <chrono>
#include <ctime>
#include <cinttypes>

#include "cache/rate_limiter.h"
#include "library/event_manager.h"
#include "library/request.h"
#include "library/byte_array.h"

namespace mydb
{
    class CacheBuffer{
        public:
        CacheBuffer(uint64_t max_buf_size, EventManager* event_manager_in): rate_limiter() {  // we can not copy rate_limiter because std::mutex is not copyable
            stop_requested = false;
            index_recv = 0;
            index_copy = 1;
            buffers.resize(2);
            buf_sizes.resize(2);
            buf_sizes[0] = buf_sizes[1] = 0;
            num_readers_recv = 0;
            num_readers_copy = 0;
            buffer_size = max_buf_size / 2; // evenly distribute it to two buffers
            event_manager = event_manager_in;
            thread_buffer_manager = std::thread(&CacheBuffer::RunLoop, this);
            is_closed = false;
        }

        ~CacheBuffer() { Close(); }

        Status Get(ByteArray& key, ByteArray* value_out);
        Status Put(ByteArray& key, ByteArray& chunk);
        Status Delete(ByteArray& key);
        void Flush();

        void Close(){
            std::unique_lock<std::mutex> lock(mutex_close);
            if (is_closed) return;
            is_closed = true;
            SetStop();
            Flush();
            cv_flush_begin.notify_one(); // notify RunLoop to exit while loop
            thread_buffer_manager.join();
        }

        bool isReadyToClose() {
            return stop_requested && buffers[index_recv].empty() && buffers[index_copy].empty();
        }
        bool isStopRequested() { return stop_requested; }
        void SetStop() { stop_requested = true; }

        private:
        Status WriteRequest(const TypeRequest& type, ByteArray& key, ByteArray& chunk);
        void RunLoop();

        int index_recv;
        int index_copy;
        uint64_t buffer_size;
        int num_readers_recv;
        int num_readers_copy;
        std::vector<std::vector<Request>> buffers;
        std::vector<int> buf_sizes;   // size of "key" and "value" in different buffers, unit is in byte
        bool stop_requested;
        bool is_closed;

        std::thread thread_buffer_manager;
        EventManager* event_manager;
        RateLimiter rate_limiter;

        std::mutex mutex_close;
        std::mutex mutex_recv_write_1;
        std::mutex mutex_flush_2;
        std::mutex mutex_indexes_swap_3;
        std::mutex mutex_copy_write_4;
        std::mutex mutex_copy_read_5;
        std::condition_variable cv_flush_begin;
        std::condition_variable cv_flush_fin;
        std::condition_variable cv_read_recv;
        std::condition_variable cv_read_copy;
    };
} // namespace mydb


#endif