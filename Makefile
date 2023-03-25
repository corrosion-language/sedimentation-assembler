CC=g++
CFLAGS=-Wall -Wextra -std=c++20 -g

all: elf.o main.o translate.o utility.o
	$(CC) $(CFLAGS) -o pasm elf.o main.o translate.o utility.o

elf.o: elf.cpp elf.hpp
	$(CC) $(CFLAGS) -c -o elf.o elf.cpp

main.o: main.cpp main.hpp
	$(CC) $(CFLAGS) -c -o main.o main.cpp

translate.o: translate.cpp translate.hpp
	$(CC) $(CFLAGS) -c -o translate.o translate.cpp

utility.o: utility.cpp utility.hpp
	$(CC) $(CFLAGS) -c -o utility.o utility.cpp

clean:
	rm -f *.o
