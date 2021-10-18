#include "network_test/server_def.h"

namespace mydb
{
void ServerTask::Run(std::thread::id tid) {
    
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
