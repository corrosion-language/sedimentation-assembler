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

sedimentation: $(OBJS)
	$(CC) $(CFLAGS) -o sedimentation $(OBJS)

%.o: %.cpp $(HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) sedimentation test/test
	rm -f test/*.o test/test test/test2
