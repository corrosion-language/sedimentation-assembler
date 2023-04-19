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
	{"rax", 64},  {"rbx", 64},	{"rcx", 64}, {"rdx", 64}, {"eax", 32},	{"ebx", 32},  {"ecx", 32},	{"edx", 32},  {"ax", 16},	{"bx", 16},
	{"cx", 16},	  {"dx", 16},	{"al", 8},	 {"bl", 8},	  {"cl", 8},	{"dl", 8},	  {"ah", 8},	{"bh", 8},	  {"ch", 8},	{"dh", 8},
	{"rdi", 64},  {"rsi", 64},	{"rbp", 64}, {"rsp", 64}, {"edi", 32},	{"esi", 32},  {"ebp", 32},	{"esp", 32},  {"di", 16},	{"si", 16},
	{"bp", 16},	  {"sp", 16},	{"dil", 8},	 {"sil", 8},  {"bpl", 8},	{"spl", 8},	  {"r8", 64},	{"r9", 64},	  {"r10", 64},	{"r11", 64},
	{"r12", 64},  {"r13", 64},	{"r14", 64}, {"r15", 64}, {"r8d", 32},	{"r9d", 32},  {"r10d", 32}, {"r11d", 32}, {"r12d", 32}, {"r13d", 32},
	{"r14d", 32}, {"r15d", 32}, {"r8w", 16}, {"r9w", 16}, {"r10w", 16}, {"r11w", 16}, {"r12w", 16}, {"r13w", 16}, {"r14w", 16}, {"r15w", 16},
	{"r8b", 8},	  {"r9b", 8},	{"r10b", 8}, {"r11b", 8}, {"r12b", 8},	{"r13b", 8},  {"r14b", 8},	{"r15b", 8},
};

static const std::unordered_map<std::string, short> _reg_num{
	{"rax", 0},	  {"rcx", 1},	{"rdx", 2},	  {"rbx", 3},	{"eax", 0},	  {"ecx", 1},	{"edx", 2},	  {"ebx", 3},	{"ax", 0},	  {"cx", 1},
	{"dx", 2},	  {"bx", 3},	{"al", 0},	  {"cl", 1},	{"dl", 2},	  {"bl", 3},	{"ah", 0},	  {"ch", 1},	{"dh", 2},	  {"bh", 3},
	{"rsp", 4},	  {"rbp", 5},	{"rsi", 6},	  {"rdi", 7},	{"esp", 4},	  {"ebp", 5},	{"esi", 6},	  {"edi", 7},	{"sp", 4},	  {"bp", 5},
	{"si", 6},	  {"di", 7},	{"spl", 4},	  {"bpl", 5},	{"sil", 6},	  {"dil", 7},	{"r8", 8},	  {"r9", 9},	{"r10", 10},  {"r11", 11},
	{"r12", 12},  {"r13", 13},	{"r14", 14},  {"r15", 15},	{"r8d", 8},	  {"r9d", 9},	{"r10d", 10}, {"r11d", 11}, {"r12d", 12}, {"r13d", 13},
	{"r14d", 14}, {"r15d", 15}, {"r8w", 8},	  {"r9w", 9},	{"r10w", 10}, {"r11w", 11}, {"r12w", 12}, {"r13w", 13}, {"r14w", 14}, {"r15w", 15},
	{"r8b", 8},	  {"r9b", 9},	{"r10b", 10}, {"r11b", 11}, {"r12b", 12}, {"r13b", 13}, {"r14b", 14}, {"r15b", 15},
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
