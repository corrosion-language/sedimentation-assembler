#pragma once

#ifndef DEFINES_HPP
#define DEFINES_HPP

#include <stdint.h>
#include <string>

enum sect { UNDEF, TEXT, DATA, BSS };

enum op_type { INVALID, REG, MEM, IMM };

// absolute address, rip relative, plt entry
// R_AMD64_32, R_AMD64_PC32/8, R_AMD64_PLT32
enum reloc_type { NONE, ABS, REL, PLT };

struct reloc_entry {
	uint64_t offset;
	int64_t addend;
	reloc_type type;
	std::string symbol;
	short size;
};

const static short _sizes[] = {8, 16, 32, 64, 128};

struct mem_output {
	std::pair<std::string, enum reloc_type> reloc = {"", NONE};
	uint8_t prefix = 0;
	uint8_t rex = 0;
	uint16_t rm = 0x7fff;
	uint16_t sib = 0x7fff;
	uint8_t offsize = 0;
	int32_t offset;
};

#endif
