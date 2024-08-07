#pragma once
#ifndef ELF_HPP
#define ELF_HPP

#include "defines.hpp"

extern std::vector<std::string> extern_labels;
extern std::unordered_set<std::string> global;
extern std::vector<struct RelocEntry> relocations;
extern std::unordered_map<std::string, std::pair<Section, size_t>> labels;
extern std::string text_buffer;
extern std::string data_buffer;
extern std::string rodata_buffer;

struct elf_header {
	uint8_t ident[16];
	uint16_t type, machine;
	uint32_t version;
	uint64_t entry, phoff, shoff;
	uint32_t flags;
	uint16_t ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
};

struct elf_program_header {
	uint32_t type, flags;
	uint64_t offset, vaddr, paddr, filesz, memsz, align;
};

struct elf_section_header {
	uint32_t name, type;
	uint64_t flags, addr, offset, size;
	uint32_t link, info;
	uint64_t addralign, entsize;
};

struct elf_symbol {
	uint32_t name;
	uint8_t info, other;
	uint16_t shndx;
	uint64_t value, size;
};

struct elf_relocation {
	uint64_t offset, info;
	int64_t addend;
};

void generate_elf(std::ofstream &, uint64_t);

#endif
