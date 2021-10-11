#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <mutex>
#include <thread>
#include <condition_variable>
#include <queue>
#include <vector>

#include "network_test/task.h"

namespace mydb
{
class ThreadPool{
public:
    ThreadPool(int num_threads): num_threads_(num_threads), is_stopped_(false) {}
    
    ~ThreadPool() {}

    void RunLoop() {
        Task* task;
        while(!is_stopped_) {
            std::unique_lock<std::mutex> lock_thread(mutex_thread_);
            if(q_.empty())  cv_thread_.wait(lock_thread);
            if(q_.empty()) continue;
            task = q_.front();
            q_.pop();
            lock_thread.unlock();

            task->Run();
            delete task;
        }
    }

    void Start() {
        threads_.resize(num_threads_);
        for(int i=0; i<num_threads_; i++) threads_[i] = std::thread(&ThreadPool::RunLoop, this);
    }

    void AddTask(Task* task) {
        std::unique_lock<std::mutex> lock_queue(mutex_queue_);
        if(task != nullptr){
            q_.push(task);
            cv_thread_.notify_one();
        }
    }

    void Stop() {
        std::unique_lock<std::mutex> lock_queue(mutex_queue_);  // protect the queue such that no one can add task into it
        is_stopped_ = true;
        cv_thread_.notify_all();
        for(auto& thd : threads_) thd.join();
        while(!q_.empty()){
            delete q_.front();
            q_.pop();
        }
    }

    void StopAfterAllTasksDone() {
        while(true){
            std::unique_lock<std::mutex> lock_queue(mutex_queue_);
            if(q_.empty()) break;
            else{
                lock_queue.unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
        }
        Stop();
    }

private:
    int num_threads_;
    bool is_stopped_;
    std::queue<Task*> q_;
    std::vector<std::thread> threads_;
    std::mutex mutex_queue_;
    std::mutex mutex_thread_;
    std::condition_variable cv_thread_;
};

} // namespace mydb

#endif  