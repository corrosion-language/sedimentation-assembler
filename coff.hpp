#pragma once
#ifndef COFF_HPP
#define COFF_HPP

#include "defines.hpp"
#include "main.hpp"

extern std::vector<std::string> extern_labels;
extern std::unordered_set<std::string> global;
extern std::vector<struct reloc_entry> relocations;
extern std::unordered_map<std::string, std::pair<sect, size_t>> labels;
extern std::vector<uint8_t> text_buffer;
extern std::vector<uint8_t> data_buffer;

struct coff_header {
	uint16_t machine = 0x8664; // AMD64
	uint16_t sections;
	uint32_t timestamp;
	uint32_t symtab_off;
	uint32_t num_symbols;
	uint16_t opt_header_size = 0;
	uint16_t flags = 0;
};

struct coff_section_header {
	char name[8];
	uint32_t vsize = 0;
	uint32_t vaddr = 0;
	uint32_t size;
	uint32_t offset;
	uint32_t reloc_off;
	uint32_t line_off = 0;
	uint16_t num_relocs;
	uint16_t num_lines = 0;
	uint32_t flags;
};

struct coff_symbol {
	char name[8];
	uint32_t val;
	uint16_t section;
	uint16_t type = 0;
	uint8_t storage_class = 0;
	uint8_t aux_symbols = 0;
} __attribute__((packed));

struct coff_relocation {
	uint32_t vaddr;
	uint32_t sym;
	uint16_t type;
} __attribute__((packed));

void generate_coff(std::ofstream &, uint64_t);

#endif
