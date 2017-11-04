# CS438 - spring 2013 MP0
#
# This is a simple example of a makefile, use this for reference when you create your own
#
# NOTE: if you decide to write your solution in C++, you will have to change the compiler
# in this file.

CXX=g++
CXX_OPTS=-g3 -pthread -O3 -pedantic -Wall -Wextra -std=c++11 -pipe

all: reliable_sender reliable_receiver

reliable_sender: sender_main.cpp utils.cpp
	${CXX} ${CXX_OPTS} -o reliable_sender sender_main.cpp utils.cpp

reliable_receiver: receiver_main.cpp utils.cpp
	${CXX} ${CXX_OPTS} -o reliable_receiver receiver_main.cpp utils.cpp

.PHONY: clean
clean:
	rm -rf *.o reliable_sender reliable_receiver
