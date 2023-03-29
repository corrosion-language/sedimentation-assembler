CC=g++
CFLAGS=-Wall -Wextra -std=c++20 -g
SRCS=$(wildcard *.cpp)
OBJS=$(SRCS:.cpp=.o)

debug: sedimentation
	./sedimentation test.asm

release: CFLAGS += -O3
release: sedimentation
	strip -s sedimentation

sedimentation: $(OBJS)
	$(CC) $(CFLAGS) -o sedimentation $(OBJS)

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) sedimentation
