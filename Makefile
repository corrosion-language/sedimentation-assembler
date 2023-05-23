SHELL=/bin/bash

ifeq ($(OS),Windows_NT)
	OUTFILE=sedimentation.exe
else
	OUTFILE=sedimentation
endif

CC=g++
CFLAGS=-Wall -Wextra -Wpedantic -std=c++20 -g
SRCS=$(wildcard *.cpp)
HDRS=$(wildcard *.hpp)
OBJS=$(SRCS:.cpp=.o)

.PHONY: debug release test clean

debug: sedimentation

release: CFLAGS += -O2
release: sedimentation
	strip -s $(OUTFILE)

sedimentation: $(OBJS)
	$(CC) $(CFLAGS) -o sedimentation $(OBJS)

translate.o: translate.cpp instr.dat
vex.o: vex.cpp vex.dat

%.o: %.cpp %.hpp Makefile
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) sedimentation test/test
	rm -f test/{a.out,*.o}
