#include <network_test/client_test.h>

int main() {
    std::string host = "--SERVER=127.0.0.1:23333";  // port number is 23333
    int num_threads = 5;
    int num_puts = 10;
    int num_dels = 5;
    int num_gets = 15;

    mydb::ThreadPool thread_pool(num_threads);
    thread_pool.Start();
    for(int i=0; i<num_threads; i++){
        mydb::ClientTask* client_task = new mydb::ClientTask(host, num_puts, num_dels, num_gets);
        thread_pool.AddTask(client_task);
    }
    thread_pool.StopAfterAllTasksDone();

    return 0;
}