#ifndef SERVER_DEF_H
#define SERVER_DEF_H

#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#include "network_test/task.h"
#include "interface/database.h"
#include "network_test/thread_pool.h"

namespace mydb
{
class ServerTask : public Task {
public:
    ServerTask(int socket_fd, Database *db): socket_fd_(socket_fd), db_(db) {}

    virtual ~ServerTask() {}

    void Run(std::thread::id tid);
private:
    int socket_fd_;
    Database *db_;
};

class Server {
public:
    Server(const std::string& db_name): db_name_(db_name) {
        is_stop_requested_ = false;
        listen_socket_fd_ = 0;
        send_stop_socket_fd_ = 0;
        recv_stop_socket_fd_ = 0;
    }

    void Start() {
        db_ = new Database(db_name_);
        db_->Open();
        thread_pool_ = new ThreadPool(10);
        listen_thread_ = std::thread(&Server::ListenLoop, this);
    }

    void ListenLoop();

    void Stop() {
        is_stop_requested_ = true;
        if(send_stop_socket_fd_ > 0){
            write(send_stop_socket_fd_, "1", 1);
        }
        listen_thread_.join();
        if(thread_pool_ != nullptr){
            thread_pool_->Stop();
            delete thread_pool_;
        }
        if(db_ != nullptr){
            db_->Close();
            delete db_;
        }
        if(listen_socket_fd_ > 0) close(listen_socket_fd_);
        if(send_stop_socket_fd_ > 0) close(send_stop_socket_fd_);
        if(recv_stop_socket_fd_ > 0) close(recv_stop_socket_fd_);
    }

private:
    bool is_stop_requested_;
    std::thread listen_thread_;
    std::string db_name_;
    Database *db_;
    ThreadPool *thread_pool_;

    int listen_socket_fd_;
    int send_stop_socket_fd_;
    int recv_stop_socket_fd_;
};

} // namespace mydb

#endif