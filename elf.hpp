#pragma once
#ifndef ELF_HPP
#define ELF_HPP

#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdint.h>
#include <string.h>
#include <vector>

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

void generate(std::ofstream &, std::vector<uint8_t> &, uint64_t, uint64_t);

#endif
