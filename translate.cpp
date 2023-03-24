#include "translate.hpp"

// utility functions

static const std::unordered_map<std::string, uint8_t> _reg_size{
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

int reg_num(std::string s) {
	uint8_t best;
	std::string best_match = "";
	for (auto &i : _reg_num) {
		if (strstr(s.c_str(), i.first.c_str()) != nullptr)
			if (i.first.length() > best_match.length()) {
				best = i.second;
				best_match = i.first;
			}
	}
	if (best_match == "")
		return -1;
	return best;
}

int reg_size(std::string s) {
	for (auto &i : _reg_size) {
		if (std::regex_match(s, (std::regex)i.first))
			return i.second;
	}
	return -1;
}

enum op_type op_type(std::string s) {
	// if in register table return REG
	if (reg_size(s) != -1)
		return REG;
	// in format "SIZE [...]" or "[]" return MEM or OFFSET
	if (s.find(' ') != std::string::npos || s[0] == '[') {
		// if in symbol table return OFF (offset)
		size_t l = s.find('[');
		size_t r = s.find(']');
		if (l == std::string::npos || r == std::string::npos)
			return INVALID;
		std::string sym = s.substr(l + 1, r - l - 1);
		// if we are using something related to a register as an offset, return MEM
		if (reg_size(s.substr(l + 1, 2)) != -1 || reg_size(s.substr(l + 1, 3)) != -1)
			return MEM;
		// otherwise return OFF (offset)
		return OFF;
	}
	// otherwise return IMM (immediate)
	return IMM;
}

std::vector<uint8_t> parse_mem(std::string in) {}

// instruction handlers

bool global(std::vector<std::string> &args) {
	if (args.size() != 1)
		return false;
	return true;
}

bool mov(std::vector<std::string> &args) {
	if (args.size() != 2)
		return false;
	// check argument types
	enum op_type t1 = op_type(args[0]);
	enum op_type t2 = op_type(args[1]);
	// if either is invalid, error
	if (t1 == INVALID || t2 == INVALID) {
		return false;
	} else if (t1 == REG && t2 == REG) {
		int s1 = reg_size(args[0]);
		int s2 = reg_size(args[1]);
		if (s1 != s2)
			return false;
		int a1 = reg_num(args[0]);
		int a2 = reg_num(args[1]);
		if (s1 == 8) {
			if (a1 >= 4 || a2 >= 4) {
				if (args[0][1] == 'h' || args[1][1] == 'h')
					return false;
				a1 += (args[0][1] == 'h') * 4;
				a2 += (args[1][1] == 'h') * 4;
				output_buffer.push_back(0x40 | ((a1 & 8) >> 3) | ((a2 & 8) >> 1));
			}
			output_buffer.push_back(0x88);
			output_buffer.push_back(0xc0 | (a2 << 3) | a1);
		} else if (s1 == 16) {
			output_buffer.push_back(0x66);
			if ((a1 | a2) & 8)
				output_buffer.push_back(0x40 | ((a1 & 8) >> 3) | ((a2 & 8) >> 1));
			output_buffer.push_back(0x89);
			output_buffer.push_back(0xc0 | (a2 << 3) | a1);
		} else if (s1 == 32) {
			if ((a1 | a2) & 8)
				output_buffer.push_back(0x40 | ((a1 & 8) >> 3) | ((a2 & 8) >> 1));
			output_buffer.push_back(0x89);
			output_buffer.push_back(0xc0 | (a2 << 3) | a1);
		} else if (s1 == 64) {
			output_buffer.push_back(0x48 | ((a1 & 8) >> 3) | ((a2 & 8) >> 1));
			output_buffer.push_back(0x89);
			output_buffer.push_back(0xc0 | (a2 << 3) | a1);
		}
	} else if (t1 == MEM && t2 == REG) {
		// too hard, not implemented yet
		std::vector<uint8_t> data = parse_mem(args[0]);
		return false;
	} else if (t1 == REG && t2 == MEM) {
		// too hard, not implemented yet
		std::vector<uint8_t> data = parse_mem(args[0]);
		return false;
	} else if (t1 == REG && t2 == OFF) {
		// hard, not implemented yet
		return false;
	} else if (t1 == OFF && t2 == REG) {
		// hard, not implemented yet
		return false;
	} else if (t1 == REG && t2 == IMM) {
		// hard, not implemented yet
		return false;
	} else if (t1 == MEM && t2 == IMM) {
		// too hard, not implemented yet
		return false;
	} else if (t1 == OFF && t2 == IMM) {
		// too hard, not implemented yet
		return false;
	} else {
		return false;
	}
	return true;
}

bool syscall(std::vector<std::string> &args) {
	if (args.size() != 0)
		return false;
	output_buffer.push_back(0x0f);
	output_buffer.push_back(0x05);
	return true;
}

bool _xor(std::vector<std::string> &args) {
	if (args.size() != 2)
		return false;
	return true;
}

// instruction, handler
const std::unordered_map<std::string, handler> _handlers{
	{"global", global},
	{"mov", mov},
	{"syscall", syscall},
	{"xor", _xor},
};

bool handle(std::string &s, std::vector<std::string> &args, size_t linenum) {
	if (_handlers.find(s) == _handlers.end()) {
		std::cerr << input_name << ':' << linenum << ": error: unknown instruction " << s << std::endl;
		return false;
	}
	if (!_handlers.at(s)(args)) {
		std::cerr << input_name << ':' << linenum << ": error: invalid combination of opcode and operands" << std::endl;
		return false;
	}
	return true;
}
