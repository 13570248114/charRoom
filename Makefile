MUDUO_DIRECTORY ?= $(HOME)/build/release-install
#MUDUO_DIRECTORY ?= $(HOME)/build/install
MUDUO_INCLUDE = $(MUDUO_DIRECTORY)/include
MUDUO_LIBRARY = $(MUDUO_DIRECTORY)/lib
SRC = .
BASE = pubsub.o codec.o

CXXFLAGS = -g -O0 -Wall -Wextra -Werror \
	   -Wconversion -Wno-unused-parameter \
	   -Wold-style-cast -Woverloaded-virtual \
	   -Wpointer-arith -Wshadow -Wwrite-strings \
	   -march=native -rdynamic -std=c++11\
	   -I$(MUDUO_INCLUDE) 

LDFLAGS = -L$(MUDUO_LIBRARY) -lmuduo_net -lmuduo_base -lpthread -lrt

clean:
	rm -f client
	rm -f hub
	rm -f *.o

hub: myhub.o codec.o
	g++ $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

client: myClient.o $(BASE)
	g++ $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

.PHONY: all clean
