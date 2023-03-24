CC=g++
CFLAGS=-Wall -Wextra -std=c++20 -g

all: main.o translate.o elf.o
	$(CC) $(CFLAGS) -o pasm main.o translate.o elf.o

elf.o: elf.cpp elf.hpp
	$(CC) $(CFLAGS) -c -o elf.o elf.cpp

translate.o: translate.cpp translate.hpp
	$(CC) $(CFLAGS) -c -o translate.o translate.cpp

main.o: main.cpp main.hpp
	$(CC) $(CFLAGS) -c -o main.o main.cpp

clean:
	rm -f *.o
