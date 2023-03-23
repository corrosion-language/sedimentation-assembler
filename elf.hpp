#pragma once
#ifndef ELF_HPP
#define ELF_HPP

#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdint.h>
#include <string.h>
#include <vector>

struct elf_header {
	uint8_t ident[16];
	uint16_t type, machine;
	uint32_t version;
	uint64_t entry, phoff, shoff;
	uint32_t flags;
	uint16_t ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
};

struct program_header {
	uint32_t type, flags;
	uint64_t offset, vaddr, paddr, filesz, memsz, align;
};

struct section_header {
	uint32_t name, type;
	uint64_t flags, addr, offset, size;
	uint32_t link, info;
	uint64_t addralign, entsize;
};

struct symbol {
	uint32_t name;
	uint8_t info, other;
	uint16_t shndx;
	uint64_t value, size;
};

void generate_elf(std::ofstream &, std::vector<uint8_t> &, uint64_t, uint64_t);

#endif
