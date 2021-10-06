CC = g++
INCLUDES = -I/usr/local/include/ -I/opt/local/include/ -I. -I./include/
LDFLAGS = -g -L/usr/local/lib/ -L/opt/local/lib -L./bin -lpthread
SOURCES = cache/cache_buffer.cpp external/encoding.cpp external/murmurhash3.cpp interface/database.cpp library/status.cpp
OBJECTS = $(SOURCES:.cpp=.o)
LIBRARY = libarbordb.a
LIBDIR = bin/

CFLAGS = -Wall -std=c++17 -MMD -MP -c -O2

default: $(SOURCES) $(LIBRARY)
default: 
	mv libarbordb.a $(LIBDIR)

local_test:
	g++ -std=c++17 -I/usr/local/include/ -I/opt/local/include/ -I./ -L/usr/local/lib/ -L/opt/local/lib -Lbin -larbordb local_test/test_main.cpp -o test_main

$(LIBRARY): $(OBJECTS)
	rm -rf $@
	ar -rs $@ $(OBJECTS)

.cpp.o:
	$(CC) $(CFLAGS) $(INCLUDES) $< -o $@

clean:
	rm -rf */*.d
	rm -rf */*.o
	rm -rf bin/*

.PHONY: local_test

-include $(SOURCES:%.cpp=%.d)