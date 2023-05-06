#pragma once
#ifndef DEFINES_HPP
#define DEFINES_HPP

#include <stdint.h>
#include <string>
#include <unordered_map>

enum sect { UNDEF, TEXT, DATA, BSS };

enum op_type { INVALID, REG, MEM, IMM };

// absolute address, rip relative, plt entry
// R_AMD64_32, R_AMD64_PC32/8, R_AMD64_PLT32
enum reloc_type { NONE, ABS, REL, PLT };

static const std::unordered_map<std::string, short> _reg_size{
	{"rax", 64},	{"rbx", 64},	{"rcx", 64},	{"rdx", 64},   {"eax", 32},	   {"ebx", 32},	   {"ecx", 32},	   {"edx", 32},	   {"ax", 16},
	{"bx", 16},		{"cx", 16},		{"dx", 16},		{"al", 8},	   {"bl", 8},	   {"cl", 8},	   {"dl", 8},	   {"ah", 8},	   {"bh", 8},
	{"ch", 8},		{"dh", 8},		{"rdi", 64},	{"rsi", 64},   {"rbp", 64},	   {"rsp", 64},	   {"edi", 32},	   {"esi", 32},	   {"ebp", 32},
	{"esp", 32},	{"di", 16},		{"si", 16},		{"bp", 16},	   {"sp", 16},	   {"dil", 8},	   {"sil", 8},	   {"bpl", 8},	   {"spl", 8},
	{"r8", 64},		{"r9", 64},		{"r10", 64},	{"r11", 64},   {"r12", 64},	   {"r13", 64},	   {"r14", 64},	   {"r15", 64},	   {"r8d", 32},
	{"r9d", 32},	{"r10d", 32},	{"r11d", 32},	{"r12d", 32},  {"r13d", 32},   {"r14d", 32},   {"r15d", 32},   {"r8w", 16},	   {"r9w", 16},
	{"r10w", 16},	{"r11w", 16},	{"r12w", 16},	{"r13w", 16},  {"r14w", 16},   {"r15w", 16},   {"r8b", 8},	   {"r9b", 8},	   {"r10b", 8},
	{"r11b", 8},	{"r12b", 8},	{"r13b", 8},	{"r14b", 8},   {"r15b", 8},	   {"xmm0", 128},  {"xmm1", 128},  {"xmm2", 128},  {"xmm3", 128},
	{"xmm4", 128},	{"xmm5", 128},	{"xmm6", 128},	{"xmm7", 128}, {"xmm8", 128},  {"xmm9", 128},  {"xmm10", 128}, {"xmm11", 128}, {"xmm12", 128},
	{"xmm13", 128}, {"xmm14", 128}, {"xmm15", 128}, {"ymm0", 256}, {"ymm1", 256},  {"ymm2", 256},  {"ymm3", 256},  {"ymm4", 256},  {"ymm5", 256},
	{"ymm6", 256},	{"ymm7", 256},	{"ymm8", 256},	{"ymm9", 256}, {"ymm10", 256}, {"ymm11", 256}, {"ymm12", 256}, {"ymm13", 256}, {"ymm14", 256},
	{"ymm15", 256},
};

static const std::unordered_map<std::string, short> _reg_num{
	{"rax", 0},	   {"rcx", 1},	  {"rdx", 2},	 {"rbx", 3},	{"eax", 0},	   {"ecx", 1},	  {"edx", 2},	 {"ebx", 3},	{"ax", 0},	   {"cx", 1},
	{"dx", 2},	   {"bx", 3},	  {"al", 0},	 {"cl", 1},		{"dl", 2},	   {"bl", 3},	  {"ah", 0},	 {"ch", 1},		{"dh", 2},	   {"bh", 3},
	{"rsp", 4},	   {"rbp", 5},	  {"rsi", 6},	 {"rdi", 7},	{"esp", 4},	   {"ebp", 5},	  {"esi", 6},	 {"edi", 7},	{"sp", 4},	   {"bp", 5},
	{"si", 6},	   {"di", 7},	  {"spl", 4},	 {"bpl", 5},	{"sil", 6},	   {"dil", 7},	  {"r8", 8},	 {"r9", 9},		{"r10", 10},   {"r11", 11},
	{"r12", 12},   {"r13", 13},	  {"r14", 14},	 {"r15", 15},	{"r8d", 8},	   {"r9d", 9},	  {"r10d", 10},	 {"r11d", 11},	{"r12d", 12},  {"r13d", 13},
	{"r14d", 14},  {"r15d", 15},  {"r8w", 8},	 {"r9w", 9},	{"r10w", 10},  {"r11w", 11},  {"r12w", 12},	 {"r13w", 13},	{"r14w", 14},  {"r15w", 15},
	{"r8b", 8},	   {"r9b", 9},	  {"r10b", 10},	 {"r11b", 11},	{"r12b", 12},  {"r13b", 13},  {"r14b", 14},	 {"r15b", 15},	{"xmm0", 0},   {"xmm1", 1},
	{"xmm2", 2},   {"xmm3", 3},	  {"xmm4", 4},	 {"xmm5", 5},	{"xmm6", 6},   {"xmm7", 7},	  {"xmm8", 8},	 {"xmm9", 9},	{"xmm10", 10}, {"xmm11", 11},
	{"xmm12", 12}, {"xmm13", 13}, {"xmm14", 14}, {"xmm15", 15}, {"ymm0", 0},   {"ymm1", 1},	  {"ymm2", 2},	 {"ymm3", 3},	{"ymm4", 4},   {"ymm5", 5},
	{"ymm6", 6},   {"ymm7", 7},	  {"ymm8", 8},	 {"ymm9", 9},	{"ymm10", 10}, {"ymm11", 11}, {"ymm12", 12}, {"ymm13", 13}, {"ymm14", 14}, {"ymm15", 15},
};

const static short _sizes[] = {-1, 8, -1, 32, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 64, -1, -1, 80, -1, -1, 16, 128, 256, 512};

struct reloc_entry {
	uint64_t offset = 0;
	int64_t addend = 0;
	reloc_type type = NONE;
	std::string symbol = nullptr;
	short size = 0;
};

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
