CC=g++
CFLAGS=-Wall -Wextra -std=c++20 -g
SRCS=$(wildcard *.cpp)
HDRS=$(wildcard *.hpp)
OBJS=$(SRCS:.cpp=.o)

.PHONY: debug release test clean

debug: sedimentation
	./sedimentation test.asm

release: CFLAGS += -O2
release: sedimentation
	strip -s sedimentation

.ONESHELL:
test: release
	cd test
	../sedimentation testfile.asm -o test
	./test | diff - output.txt
	rm test
	../sedimentation testc.asm -o test.o -c
	gcc -nostartfiles test.o -o test
	./test | diff - outputc.txt

sedimentation: $(OBJS)
	$(CC) $(CFLAGS) -o sedimentation $(OBJS)

%.o: %.cpp $(HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) sedimentation test/test
