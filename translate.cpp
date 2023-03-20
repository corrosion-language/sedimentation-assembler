#include "translate.hpp"

// utility functions

int reg_size(std::string s) {
	// nx, nl, nh, rn, di, si
	if (s.size() == 2) {
		if (s[0] == 'r')
			return 64;
		if (s[1] == 'x' || s[1] == 'i')
			return 16;
		return 8;
	}
	// rnx
	if (s[0] == 'r' && s[2] == 'x')
		return 64;
	if (s[0] == 'e')
		return 32;
	if (s[2] == 'l' || s[2] == 'h')
		return 8;
	if (s[2] == 'w')
		return 16;
	if (s[2] == 'd')
		return 32;
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

// instruction handlers

bool global(std::vector<std::string> &args) {
	if (args.size() != 1)
		return false;
	return true;
}

bool mov(std::vector<std::string> &args) {
	return true;
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
		// doable, not implemented yet
	} else if (t1 == REG && t2 == MEM) {
		// too hard, not implemented yet
		return false;
	} else if (t1 == MEM && t2 == REG) {
		// too hard, not implemented yet
		return false;
	} else if (t1 == REG && t2 == IMM) {
		// hard, not implemented yet
		return false;
	} else if (t1 == REG && t2 == OFF) {
		// hard, not implemented yet
		return false;
	} else if (t1 == MEM && t2 == IMM) {
		// too hard, not implemented yet
		return false;
	} else if (t1 == OFF && t2 == REG) {
		// hard, not implemented yet
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
std::unordered_map<std::string, handler> _handlers{
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
	if (!_handlers[s](args)) {
		std::cerr << input_name << ':' << linenum << ": error: invalid combination of opcode and operands" << std::endl;
		return false;
	}
	return true;
}
