#include <regex>
#include <string>
#include <unordered_map>

enum op_type { INVALID, REG, MEM, IMM, OFF };
enum sect { UNDEF, TEXT, RODATA, BSS };

static const std::unordered_map<std::string, int> _reg_size{
	{"((?:r[89])|(?:r1[0-5]))b", 8}, // r8b - r15b
	{"((?:r[89])|(?:r1[0-5]))w", 16}, // r8w - r15w
	{"((?:r[89])|(?:r1[0-5]))d", 32}, // r8d - r15d
	{"(?:r[89])|(?:r1[0-5])", 64}, // r8 - r15
	{"[a-d][hl]", 8}, // al-dh
	{"(?:di|si|sp|bp)l", 8}, // dil sil spl bpl
	{"di|si|sp|bp", 16}, // di si sp bp
	{"[a-d]x", 16}, // ax - dx
	{"e[a-d]x", 32}, // eax - edx
	{"e(?:di|si|sp|bp)", 32}, // edi esi esp ebp
	{"r[a-d]x", 64}, // rax - rdx
	{"r(?:di|si|sp|bp)", 64}, // rdi rsi rsp rbp
};

static const std::unordered_map<std::string, uint8_t> _reg_num{
	{"a", 0},  {"c", 1},  {"d", 2},	   {"b", 3},	{"sp", 4},	 {"bp", 5},	  {"si", 6},   {"di", 7},
	{"r8", 8}, {"r9", 9}, {"r10", 10}, {"r11", 11}, {"r12", 12}, {"r13", 13}, {"r14", 14}, {"r15", 15},
};

int reg_num(std::string);
int reg_size(std::string);
enum op_type op_type(std::string);
std::vector<uint8_t> parse_mem(std::string);
std::vector<uint8_t> parse_off(std::string);
std::pair<unsigned long long, short> parse_imm(std::string);
