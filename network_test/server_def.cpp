#include <sstream>

#include "network_test/server_def.h"

namespace mydb
{
void ServerTask::Run(std::thread::id tid) {
    // here, I assumed that for tcp connection, one send() is corresponding to one recv(),
    // for larger "message" this is not true because one "message" could be splitted into 
    // multiple "segments" (TCP packets) and recv buffer may not be large enough to receive
    // one packet in one recv() call.

    // If I have time in the future, I will improve this part to make it more robust and generic
    std::stringstream ss;
    ss << tid;

    std::cout << "Start task on thread id : " << ss.str() << std::endl;

    char key[512];
    char val[1024];
    char buffer_recv[1024];
    char buffer_send[1024];
    std::regex regex_put("set ([\\S]*) \\d* \\d* (\\d*)\r\n");
    bool is_get, is_put, is_del;
    uint32_t size_key, size_val;
    uint32_t bytes_received;
    while(true){
        is_get = is_put = is_del = false;
        bytes_received = recv(socket_fd_, buffer_recv, 1024, 0);
        if(bytes_received <= 0) break;
        if(memcmp(buffer_recv, "get", 3) == 0) is_get = true;
        else if(memcmp(buffer_recv, "set", 3) == 0) is_put = true;
        else if(memcmp(buffer_recv, "delete", 6) == 0) is_del = true;
        else if(memcmp(buffer_recv, "quit", 4) == 0) break;

        if(is_put) {
            uint32_t offset = 4;
            while(buffer_recv[offset] != ' ') offset += 1;
            size_key = offset - 4;
            memcpy(key, buffer_recv + 4, size_key);
            while(buffer_recv[offset] != '\n') offset += 1;
            offset += 1;    // for the \n

            std::smatch match;
            std::string s(buffer_recv, offset);
            std::regex_search(s, match, regex_put);
            size_val = std::stoi(match[2].str());
            memcpy(val, buffer_recv + offset, size_val);

            ByteArray _key_ = ByteArray::NewCopyByteArray(key, size_key);
            ByteArray _val_ = ByteArray::NewCopyByteArray(val, size_val);

            // std::cout << "Put key : " << _key_.ToString() << " on thread id : " << ss.str() << std::endl;
            // std::cout << "Put val : " << _val_.ToString() << " on thread id : " << ss.str() << std::endl;

            db_->Put(_key_, _val_);

            // std::string val_check;
            // db_->Get(_key_, &val_check);
            // std::cout << "Put val check : " << val_check << " on thread id : " << ss.str() << std::endl;

            send(socket_fd_, "STORED\r\n", 8, 0);
        }
        else if(is_del) {
            size_key = bytes_received - 7 - 2;
            memcpy(key, buffer_recv + 7, size_key);
            ByteArray _key_ = ByteArray::NewCopyByteArray(key, size_key);

            // std::cout << "Delete key : " << _key_.ToString() << " on thread id : " << ss.str() << std::endl;

            db_->Delete(_key_);
            send(socket_fd_, "DELETED\r\n", 9, 0);
        }
        else if(is_get) {
            size_key = bytes_received - 4 - 2;
            memcpy(key, buffer_recv + 4, size_key);
            ByteArray _key_ = ByteArray::NewReferenceByteArray(key, size_key);
            std::string _val_;
            Status s = db_->Get(_key_, &_val_);

            // std::cout << "Get key : " << _key_.ToString() << " on thread id : " << ss.str() << std::endl;
            // std::cout << "Get val : " << _val_ << " on thread id : " << ss.str() << std::endl;

            if(s.IsOK()){
                snprintf(buffer_send, 1024, "VALUE %s 0 %" PRIu64 "\r\n%s\r\n", 
                        _key_.ToString().c_str(), _val_.size(), _val_.c_str());
                send(socket_fd_, buffer_send, strlen(buffer_send), 0);
                send(socket_fd_, "END\r\n", 5, 0);
            }
            else{
                send(socket_fd_, "NOT_FOUDN\r\n", 11, 0);
            }
        }
    }

    close(socket_fd_);  // close connection to client socket because we have already finished client task
}

void Server::ListenLoop() {
    signal(SIGPIPE, SIG_IGN);
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;        // accept both ipv4 and ipv6
    hints.ai_socktype = SOCK_STREAM;    // TCP type
    hints.ai_flags = AI_PASSIVE;        // passive is required for listening socket
    const char* service = "23333";      // port number is 23333

    getaddrinfo(NULL, service, &hints, &res);
    // set node to NULL so that the network address will be lopback address

    struct addrinfo *rp;
    for(rp = res; rp != NULL; rp = rp->ai_next) {
        if((listen_socket_fd_ = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) == -1) continue;
        int do_reuse_address = 1;
        setsockopt(listen_socket_fd_, SOL_SOCKET, SO_REUSEADDR, &do_reuse_address, sizeof(do_reuse_address));
        if(bind(listen_socket_fd_, rp->ai_addr, rp->ai_addrlen) == -1) continue;
        break;
    }
    freeaddrinfo(res);
    if(rp == NULL){
        std::cout << "Can not bind listen socket to assigned port" << std::endl;
        is_stop_requested_ = true;
        return;
    }

    listen(listen_socket_fd_, 100);

    int pipefd[2];  // used to notify listen socket to close
    pipe(pipefd);
    recv_stop_socket_fd_ = pipefd[0];
    send_stop_socket_fd_ = pipefd[1];
    fcntl(send_stop_socket_fd_, F_SETFL, O_NONBLOCK);   // set file status flag to nonblock, 
                                                        // the process won't wait for return of system call
    fd_set set_read;    // used to monitoring whether if sockets are read-ready
    int nfds = std::max(recv_stop_socket_fd_, listen_socket_fd_) + 1;

    // Now, we are ready to listen (start the processing loop)
    int socket_accept_;
    while(!is_stop_requested_) {
        FD_ZERO(&set_read);
        FD_SET(recv_stop_socket_fd_, &set_read);
        FD_SET(listen_socket_fd_, &set_read);

        if(select(nfds, &set_read, NULL, NULL, NULL) == 0) continue;

        if(!FD_ISSET(listen_socket_fd_, &set_read)) continue;   // we are notified to close listen socket, so do nothing

        socket_accept_ = accept(listen_socket_fd_, NULL, NULL);
        if(socket_accept_ == -1) continue;
        thread_pool_->AddTask(new ServerTask(socket_accept_, db_));
    }
}
} // namespace mydb
