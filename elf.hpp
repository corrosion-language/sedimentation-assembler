#pragma once
#ifndef ELF_HPP
#define ELF_HPP

#include "defines.hpp"
#include "main.hpp"
#include <fstream>
#include <iostream>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

extern std::vector<std::string> extern_labels;
extern std::unordered_set<std::string> global;
extern std::vector<struct reloc_entry> relocations;
extern std::unordered_map<std::string, std::pair<sect, size_t>> labels;

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

struct relocation {
	uint64_t offset, info;
	int64_t addend;
};

void generate_elf(std::ofstream &, std::vector<uint8_t> &, uint64_t, uint64_t);

#endif
