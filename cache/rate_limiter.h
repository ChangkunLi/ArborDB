#ifndef RATE_LIMITER_H
#define RATE_LIMITER_H

#include <queue>
#include <mutex>
#include <thread>

namespace mydb
{
    class RateLimiter
    {
    public:
        RateLimiter() : writing_rate(0), received_bytes(0), time_prev(0), time_cur(0),
                        sleep_on_nbytes(10), time_write_start(0), time_write_end(0),
                        total_rates(0), max_log(10), cur_log(0) {}

        ~RateLimiter() {}

        // this method will be used by every putter/deletter thread, need to consider concurrency problem
        void Stop(uint64_t bytes_incoming){
            time_cur = std::time(nullptr);
            if(time_cur != time_prev){
                // oome time has passed, we need to update sleep_on_nbytes
                bool isFirstThread = false;
                uint64_t dt;
                uint64_t received_bytes_copy;
                std::unique_lock<std::mutex> lock(mutex_sync);
                if(time_cur != time_prev){
                    dt = time_cur - time_prev;
                    time_prev = time_cur;
                    received_bytes_copy = received_bytes;
                    received_bytes = 0;
                    isFirstThread = true;
                }
                lock.unlock();
                if(isFirstThread){
                    // update only on first thread
                    double ratio = double(received_bytes_copy) / double(getWritingRate());
                    if (ratio > 1.0) {
                        if (ratio > 1.50) {
                            sleep_on_nbytes *= 0.75;
                        } else if (ratio > 1.10) {
                            sleep_on_nbytes *= 0.95;
                        } else if (ratio > 1.05) {
                            sleep_on_nbytes*= 0.99;
                        } else {
                            sleep_on_nbytes *= 0.995;
                        }
                        if (sleep_on_nbytes<= 5) sleep_on_nbytes += 1;
                    } else {
                        if (ratio < 0.5) {
                            sleep_on_nbytes *= 1.25;
                        } else if (ratio < 0.90) {
                            sleep_on_nbytes *= 1.05;
                        } else if (ratio < 0.95) {
                            sleep_on_nbytes *= 1.01;
                        } else {
                            sleep_on_nbytes *= 1.005;
                        }
                        if (sleep_on_nbytes <= 5) sleep_on_nbytes += 1;
                    }
                }
            }

            received_bytes+= bytes_incoming;
            uint64_t t_sleep = 0;
            if(sleep_on_nbytes > 0) t_sleep = bytes_incoming / sleep_on_nbytes;
            if(t_sleep){
                std::chrono::microseconds duration(t_sleep);
                std::this_thread::sleep_for(duration);                
            }
        }

        // rest of the function will only be used by a single thread running in cache buffer to monitor the writing rate
        void startWrite() {
            time_write_start = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        }

        void endWrite(uint64_t nbytes_written) {
            time_write_end = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            uint64_t this_writing_rate;
            if(time_write_end == time_write_start){
                this_writing_rate = nbytes_written * 1000;
            }
            else{
                this_writing_rate = nbytes_written * 1000 / (time_write_end - time_write_start);
            }
            updateWritingRate(this_writing_rate);
        }

        void updateWritingRate(uint64_t rate){
            total_rates+= rate;
            log_rates.push(rate);
            if(cur_log<max_log) cur_log++;
            else{
                total_rates-= log_rates.front();
                log_rates.pop();
            }
            writing_rate = total_rates / cur_log;
        }

        uint64_t getWritingRate(){
            if(!cur_log) return 1024 * 1024;
            return writing_rate;
        }


    private:
        uint64_t writing_rate;
        uint64_t received_bytes;
        uint64_t time_prev;
        uint64_t time_cur;
        uint64_t sleep_on_nbytes; // sleep 1 microsecond for every nbytes received
        uint64_t time_write_start;
        uint64_t time_write_end;
        std::queue<uint64_t> log_rates;
        uint64_t total_rates;
        std::mutex mutex_sync;
        int max_log;
        int cur_log; 
    };
}

#endif