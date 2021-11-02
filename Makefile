CC = g++
INCLUDES = -I/usr/local/include/ -I/opt/local/include/ -I. -I./include/
LDFLAGS = -g -L/usr/local/lib/ -L/opt/local/lib -L./bin -lpthread
LDFLAGS_CLIENT = -g -L/usr/local/lib/ -L/opt/local/lib -L./bin -lpthread -lmemcached
SOURCES = cache/cache_buffer.cpp external/encoding.cpp external/murmurhash3.cpp interface/database.cpp library/status.cpp network_test/server_def.cpp
SOURCES_SERVER = network_test/server.cpp
SOURCES_CLIENT = network_test/client_test.cpp
OBJECTS = $(SOURCES:.cpp=.o)
OBJECTS_SERVER = $(SOURCES_SERVER:.cpp=.o)
OBJECTS_CLIENT = $(SOURCES_CLIENT:.cpp=.o)
SERVER = server
CLIENT = client
LIBRARY = libarbordb.a
LIBDIR = bin/

CFLAGS = -Wall -std=c++17 -MMD -MP -c -O2

default: $(SOURCES) $(LIBRARY)
default: 
	mv libarbordb.a $(LIBDIR)

local_server: $(SOURCES) $(SOURCES_SERVER) $(SERVER)

local_client: $(SOURCES) $(SOURCES_CLIENT) $(CLIENT)

local_test:
	g++ -std=c++17 -I/usr/local/include/ -I/opt/local/include/ -I./ -L/usr/local/lib/ -L/opt/local/lib -Lbin -lpthread -larbordb local_test/test_main.cpp -o test_main
	./test_main

$(LIBRARY): $(OBJECTS)
	rm -rf $@
	ar -rs $@ $(OBJECTS)

$(SERVER): $(OBJECTS) $(OBJECTS_SERVER)
	$(CC) $(OBJECTS) $(OBJECTS_SERVER) -o $@ $(LDFLAGS)

$(CLIENT): $(OBJECTS) $(OBJECTS_CLIENT)
	$(CC) $(OBJECTS) $(OBJECTS_CLIENT) -o $@ $(LDFLAGS_CLIENT)

.cpp.o:
	$(CC) $(CFLAGS) $(INCLUDES) $< -o $@

clean:
	rm -rf */*.d
	rm -rf */*.o
	rm -rf bin/*
	rm -rf *.d
	rm -f test_main
	rm -f server
	rm -f client

.PHONY: local_test

-include $(SOURCES:%.cpp=%.d)