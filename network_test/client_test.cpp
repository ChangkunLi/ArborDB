#include <network_test/client_test.h>

int main() {
    std::string host = "--SERVER=127.0.0.1:23333";  // port number is 23333
    int num_threads = 10;
    int num_puts = 1000;
    int num_dels = 500;
    int num_gets = 1500;

    mydb::ThreadPool thread_pool(num_threads);
    mydb::ClientTask* client_task = new mydb::ClientTask(host, num_puts, num_dels, num_gets);
    thread_pool.AddTask(client_task);
    thread_pool.StopAfterAllTasksDone();

    return 0;
}