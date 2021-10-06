#include "cache/cache_buffer.h"

namespace mydb
{
    void CacheBuffer::Flush() {
        std::unique_lock<std::mutex> lock(mutex_flush_2);
        if(isReadyToClose()) return;
        for(int i=0; i<2; i++){
            cv_flush_begin.notify_one();
            cv_flush_fin.wait_for(lock,std::chrono::milliseconds(500));
        }
    }

    Status CacheBuffer::Get(ByteArray& key, ByteArray* value_out) {
        if(isStopRequested()) return Status::IOError("Can not retrieve key because database has been closed");
        // check receive buffer
        mutex_recv_write_1.lock();
        mutex_indexes_swap_3.lock();
        auto& buffer_recv = buffers[index_recv];
        int n_req = buffer_recv.size();
        num_readers_recv+= 1; 
        mutex_indexes_swap_3.unlock();
        mutex_recv_write_1.unlock();
        bool found_in_recv_buffer = false;
        Request found_req;
        for(int i=n_req-1; i>=0; i--){
            Request& req = buffer_recv[i];
            if(req.key == key){
                found_in_recv_buffer = true;
                found_req = req;
                break;
            }
        }
        Status early_ret;
        if(found_in_recv_buffer){
            if(found_req.type == TypeRequest::Put){
                *value_out = found_req.val;
                early_ret = Status::OK();
            }
            else if (found_req.type == TypeRequest::Delete){
                early_ret = Status::DeleteOrder();
            }
            else early_ret = Status::NotFound("Unknown Type found in cache buffer");
        }

        // reduce number of readers
        mutex_indexes_swap_3.lock();
        num_readers_recv-= 1;
        if(!num_readers_recv) cv_read_recv.notify_one();
        mutex_indexes_swap_3.unlock();
        
        if(found_in_recv_buffer) return early_ret;

        // Grab resource in copy buffer
        mutex_copy_write_4.lock();
        mutex_copy_read_5.lock();
        num_readers_copy+= 1;
        mutex_copy_read_5.unlock();
        mutex_copy_write_4.unlock();

        // check copy buffer
        mutex_indexes_swap_3.lock();
        auto& buffer_copy = buffers[index_copy];
        mutex_indexes_swap_3.unlock();

        bool found_in_copy_buffer = false;
        int n_cpy = buffer_copy.size();
        for(int i=n_cpy-1; i>=0; i--){
            Request& req = buffer_copy[i];
            if(req.key == key){
                found_in_copy_buffer = true;
                found_req = req;
                break;
            }
        }
        Status ret;
        if(found_in_copy_buffer){
            if(found_req.type == TypeRequest::Put){
                *value_out = found_req.val;
                ret = Status::OK();
            }
            else if (found_req.type == TypeRequest::Delete){
                ret = Status::DeleteOrder();
            }
            else ret = Status::NotFound("Unknown Type found in cache buffer");
        }
        else ret = Status::NotFound("Key is not found in cache buffer, please search record on disk");

        mutex_copy_read_5.lock();
        num_readers_copy-= 1;
        if(!num_readers_copy) cv_read_copy.notify_one();
        mutex_copy_read_5.unlock();

        return ret;
    }

    Status CacheBuffer::Put(ByteArray& key, ByteArray& chunk) {
        return WriteRequest(TypeRequest::Put, key, chunk);
    }

    Status CacheBuffer::Delete(ByteArray& key) {
        ByteArray empty_ = ByteArray();  // define an empty bytearray for the purpose of placeholder
        return WriteRequest(TypeRequest::Delete, key, empty_);
    }

    Status CacheBuffer::WriteRequest(const TypeRequest& type, ByteArray& key, ByteArray& chunk) {
        if (isReadyToClose()) return Status::IOError("Can not update request because database has been closed");
        uint64_t bytes_incoming = key.size() + chunk.size();
        rate_limiter.Stop(bytes_incoming);
        // Grab key to write, because at any given time, only one writer is allowed
        std::unique_lock<std::mutex> lock_level_1(mutex_recv_write_1);
        mutex_indexes_swap_3.lock();
        buffers[index_recv].push_back(Request{std::this_thread::get_id(), type, key, chunk, chunk.size()});
        buf_sizes[index_recv]+= bytes_incoming;
        uint64_t cur_size_recv = buf_sizes[index_recv];
        mutex_indexes_swap_3.unlock();

        if(cur_size_recv > buffer_size){
            // triggers a flush
            std::unique_lock<std::mutex> lock_flush(mutex_flush_2);
            cv_flush_begin.notify_one();
        }
        
        return Status::OK();
    }

    void CacheBuffer::RunLoop() {
        while(true){
            std::unique_lock<std::mutex> lock_flush(mutex_flush_2);
            while(!buf_sizes[index_recv]){
                cv_flush_begin.wait_for(lock_flush, std::chrono::milliseconds(500));
                if(isReadyToClose()) return;
            }

            mutex_recv_write_1.lock();
            std::unique_lock<std::mutex> lock_index(mutex_indexes_swap_3);
            if(num_readers_recv > 0){
                cv_read_recv.wait(lock_index);
            }  // wait for readers in recv buffer to finish
            std::swap(index_recv, index_copy);
            lock_index.unlock();
            mutex_recv_write_1.unlock();
            event_manager->flush_buffer.StartAndBlockUntilDone(buffers[index_copy]);
            event_manager->clear_buffer.Wait();
            event_manager->clear_buffer.Done();

            mutex_copy_write_4.lock();
            std::unique_lock<std::mutex> lock_copy(mutex_copy_read_5);
            if(num_readers_copy > 0){
                cv_read_copy.wait(lock_copy);
            }  // wait for readers in copy buffer to finish
            lock_copy.unlock();
            // clear copy buffer
            buf_sizes[index_copy] = 0;
            buffers[index_copy].clear();
            mutex_copy_write_4.unlock();
            cv_flush_fin.notify_all();

            if(isReadyToClose()) return;
        }
    }

} // namespace mydb
