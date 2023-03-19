CC=g++
CFLAGS=-Wall -Wextra -std=c++20 -g

all: 
	$(CC) $(CFLAGS) -o pasm main.cpp elf.cpp translate.cpp
