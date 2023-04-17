#include <regex>
#include <string>
#include <unordered_map>

extern std::string prev_label;
extern std::unordered_map<std::string, uint64_t> data_labels;
extern std::unordered_map<std::string, uint64_t> bss_labels;
extern std::unordered_map<std::string, size_t> text_labels_map;
extern std::string error;

enum op_type { INVALID, REG, MEM, IMM };

static const std::unordered_map<std::string, short> _reg_size{
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

static const std::unordered_map<std::string, short> _reg_num{
	{"a", 0},  {"c", 1},  {"d", 2},	   {"b", 3},	{"sp", 4},	 {"bp", 5},	  {"si", 6},   {"di", 7},
	{"r8", 8}, {"r9", 9}, {"r10", 10}, {"r11", 11}, {"r12", 12}, {"r13", 13}, {"r14", 14}, {"r15", 15},
};

struct mem_output {
	short reloc = 0x7fff;
	uint8_t prefix = 0;
	uint8_t rex = 0;
	uint16_t rm = 0x7fff;
	uint16_t sib = 0x7fff;
	uint8_t offsize = 0;
	int32_t offset;
};

short reg_num(std::string);
short reg_size(std::string);
short mem_size(std::string);
enum op_type op_type(std::string);
mem_output *parse_mem(std::string, short &);
std::pair<unsigned long long, short> parse_imm(std::string);
