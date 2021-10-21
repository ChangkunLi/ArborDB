#include <csignal>
#include <execinfo.h>

#include "network_test/server_def.h"

bool stop_requested = false;

void termination_signal_handler(int signal) {
    stop_requested = true;
}

void crash_signal_handler(int sig) {
    int depth_max = 20;
    void *array[depth_max];
    size_t depth;

    depth = backtrace(array, depth_max);
    fprintf(stderr, "Error: signal %d:\n", sig);
    backtrace_symbols_fd(array, depth, STDERR_FILENO);
    exit(1);
}

int main(int argc, char**  argv) {
    signal(SIGINT, termination_signal_handler);
    signal(SIGTERM, termination_signal_handler);

    signal(SIGSEGV, crash_signal_handler);
    signal(SIGABRT, crash_signal_handler);

    std::string db_name = "network_test_db";
    mydb::Server server(db_name);
    server.Start();
    while(!stop_requested && !server.IsStopRequested()){
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    server.Stop();
    return 0;
}