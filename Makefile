SHELL=/bin/bash

CC=g++
CFLAGS=-Wall -Wextra -Wpedantic -std=c++20 -g
SRCS=$(wildcard *.cpp)
HDRS=$(wildcard *.hpp)
PCHS=$(HDRS:.hpp=.hpp.gch)
OBJS=$(SRCS:.cpp=.o)

ifeq ($(OS),Windows_NT)
	OUTFILE=sedimentation.exe
	CFLAGS += -DWINDOWS
else
	OUTFILE=sedimentation
	ifeq ($(shell uname -s),Darwin)
		CFLAGS += -DMACOS
	else
		CFLAGS += -DLINUX
	endif
endif

.PHONY: debug release test clean

debug: sedimentation

release: CFLAGS += -O2
release: sedimentation
	strip -s $(OUTFILE)

sedimentation: $(PCHS) $(OBJS)
	$(CC) $(CFLAGS) -o sedimentation $(OBJS)

translate.o: translate.cpp instr.dat
vex.o: vex.cpp vex.dat

%.hpp.gch: %.hpp Makefile
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cpp $(PCHS) Makefile
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(PCHS) sedimentation test/test
	rm -f test/{a.out,*.o}
