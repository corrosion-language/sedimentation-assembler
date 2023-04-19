CC=g++
CFLAGS=-Wall -Wextra -std=c++20 -g
SRCS=$(wildcard *.cpp)
HDRS=$(wildcard *.hpp) instr.dat
OBJS=$(SRCS:.cpp=.o)

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

sedimentation: $(OBJS)
	$(CC) $(CFLAGS) -o sedimentation $(OBJS)

%.o: %.cpp $(HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) sedimentation test/test
