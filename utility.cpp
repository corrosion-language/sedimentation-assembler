#include "utility.hpp"

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

std::vector<uint8_t> parse_off(std::string in) {}

std::pair<unsigned long long, short> parse_imm(std::string s) {
	// detect base
	int base = 0;
	if (s.starts_with("0x"))
		base = 16;
	else if (s.starts_with("0b"))
		base = 2;
	else
		base = 10;
	// parse
	bool neg = s[0] == '-';
	try {
		if (neg) {
			int64_t val = std::stoll(s, nullptr, base);
			short size = 64;
			if ((int8_t)val == val)
				size = 8;
			else if ((int16_t)val == val)
				size = 16;
			else if ((int32_t)val == val)
				size = 32;
			return {val, size};
		} else {
			uint64_t val = std::stoull(s, nullptr, base);
			short size = 64;
			if ((val & 0xffffffffffffff00) == 0)
				size = 8;
			else if ((val & 0xffffffffffff0000) == 0)
				size = 16;
			else if ((val & 0xffffffff00000000) == 0)
				size = 32;
			return {val, size};
		}
	} catch (std::invalid_argument) {
		return {0, -1};
	} catch (std::out_of_range) {
		return {0, -2};
	}
}
