# ArborDB
An on-disk persistent key-value database with concurrent design

How to make library?

'make default' or simply 'make'

How to make local test?

'make local_test'

How to use ArborDB as an external library?

Include "include/headers" e.g. #include "include/headers.h"

Usage Example: ( current path is ArborDB/ )

g++ -std=c++17 -I/usr/local/include/ -I/opt/local/include/ -I./ -L/usr/local/lib/ -L/opt/local/lib -Lbin -lpthread -larbordb local_test/test_main.cpp -o test_main

Default port number for memcached service is set to be 23333
