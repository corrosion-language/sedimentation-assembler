#include "utility.hpp"

std::string error;

short reg_num(const std::string &s) {
	auto ptr = _reg_num.find(s);
	if (ptr == _reg_num.end())
		return -1;
	else
		return ptr->second;
}

short reg_size(const std::string &s) {
	auto ptr = _reg_size.find(s);
	if (ptr == _reg_size.end())
		return -1;
	else
		return ptr->second;
}

short mem_size(const std::string &s) {
	if (s.starts_with("byte "))
		return 8;
	if (s.starts_with("word "))
		return 16;
	if (s.starts_with("dword "))
		return 32;
	if (s.starts_with("qword "))
		return 64;
	if (s.starts_with("tbyte "))
		return 80;
	if (s.starts_with("oword ") || s.starts_with("xword ") || s.starts_with("xmmword "))
		return 128;
	if (s.starts_with("yword ") || s.starts_with("ymmword "))
		return 256;
	return -1;
}

op_type get_optype(const std::string &s) {
	// if in register table return REG
	if (reg_size(s) != -1)
		return REG;
	// in format "SIZE [...]" or "[]" return MEM or OFFSET
	auto tmp = s.find(' ');
	tmp = (tmp != std::string::npos) ? tmp : -1;
	if (s[tmp + 1] == '[' || s[0] == '[') {
		size_t l = s.find('[');
		size_t r = s.find(']');
		if (l == std::string::npos || r == std::string::npos)
			return INVALID;
		return MEM;
	}
	// otherwise return IMM (immediate)
	return IMM;
}

// this function will NOT handle invalid input properly
mem_output *parse_mem(std::string in, short &size) {
	if (reg_size(in) != -1) {
		mem_output *out = new mem_output();
		short s1 = reg_size(in);
		short a1 = reg_num(in);
		if (s1 == 16)
			out->prefix = 0x66;
		if (a1 >= 8 || s1 == 64)
			out->rex = 0x40 | ((s1 == 64) << 3) | (a1 >= 8);
		out->rm = 0xc0 | (a1 & 7);
		return out;
	}
	// off can be: label + num, label, num
	// [reg + reg * scale + off] SIB
	// [reg + reg * scale] SIB
	// [reg + reg + off] SIB
	// [reg + reg] SIB
	// [reg * scale + off] SIB
	// [reg * scale] SIB
	// [reg + off] NO SIB
	// [reg] NO SIB
	// [off] NO SIB
	if (size == -1 || in[0] != '[') {
		short tmp;
		tmp = mem_size(in);
		if (tmp == -1)
			return nullptr;
		size = tmp;
		in = in.substr(5 + (tmp >= 32));
	}
	if (in.starts_with("[rel ")) {
		mem_output *out = new mem_output();
		out->rm = 0x05;
		out->offsize = 32;
		out->reloc.second = REL;
		in = in.substr(5, in.size() - 6);
		if (!labels.count(in)) {
			error = "symbole « " + in + " » non défini";
			return nullptr;
		}
		size_t op = in.find('+');
		if (op != std::string::npos) {
			out->offset = std::stoi(in.substr(op + 1), 0, 0);
			in = in.substr(0, op);
		} else {
			op = in.find('-');
			if (op != std::string::npos) {
				out->offset = -std::stoi(in.substr(op + 1), 0, 0);
				in = in.substr(0, op);
			} else {
				out->offset = 0;
			}
		}
		out->reloc.first = in;
		return out;
	}
	in = in.substr(0, in.size() - 1);
	std::vector<std::string> tokens;
	std::vector<char> ops;
	size_t l = 0;
	size_t r = std::min(in.find('*'), std::min(in.find('+'), in.find('-')));
	while (l++ != std::string::npos) {
		tokens.push_back(in.substr(l, r - l));
		if (l != 1)
			ops.push_back(in[l - 1]);
		l = r;
		r = std::min(in.find('*', l + 1), std::min(in.find('+', l + 1), in.find('-', l + 1)));
	}
	if (tokens.size() == 0)
		return nullptr;

	for (size_t i = 0; i < ops.size(); i++) {
		if (ops[i] == '-') {
			tokens[i + 1] = std::to_string(-std::stoi(tokens[i + 1]));
			ops[i] = '+';
		}
	}

	bool has_reg = false;
	for (size_t i = 0; i < tokens.size(); i++) {
		if (reg_size(tokens[i]) != -1) {
			has_reg = true;
			break;
		}
	}
	if (has_reg) {
		// move stuff from front to back if not register
		while (tokens.size() > 1 && reg_size(tokens.front()) == -1) {
			tokens.push_back(tokens.front());
			ops.push_back('+');
			tokens.erase(tokens.begin());
			ops.erase(ops.begin());
		}
	}

	mem_output *out = new mem_output();

	// resolve labels and combine with imms if possible
	if (tokens.size() > 1 && (data_labels.count(tokens[tokens.size() - 2]) || bss_labels.count(tokens[tokens.size() - 2]))) {
		out->reloc.first = tokens[tokens.size() - 2];
		out->reloc.second = ABS;
		if (ops.back() != '+')
			return nullptr;
		tokens.erase(tokens.end() - 2);
		ops.pop_back();
	} else {
		if (data_labels.count(tokens.front()) || bss_labels.count(tokens.front())) {
			tokens.push_back(tokens.front());
			ops.push_back('+');
			tokens.erase(tokens.begin());
			ops.erase(ops.begin());
		}
		if (data_labels.count(tokens.back()) || bss_labels.count(tokens.back())) {
			out->reloc.first = tokens.back();
			out->reloc.second = ABS;
			tokens.back() = "0";
		}
	}

	// check to see if we need to use SIB
	if (tokens.size() == 1 || (reg_size(tokens[1]) == -1 && ops[0] == '+')) {
		// no sib
		if (tokens.size() == 1) {
			short a1 = reg_num(tokens[0]);
			if (a1 != -1) {
				// register
				// size override
				if (reg_size(tokens[0]) == 32)
					out->prefix = 0x67;
				// rex prefix if necessary
				if (a1 >= 8) {
					out->rex = 0x41;
					// check for collisions with rip relative addressing
					if (a1 == 13) {
						out->rm = 0x45;
						out->offsize = 8;
						out->offset = 0x00;
						return out;
					}
					// collisions with SIB addressing
					if ((a1 & 7) == 0b100) {
						out->rm = 0x04;
						out->sib = 0b00100000 | (a1 & 7);
						return out;
					}
				}
				// modrm
				out->rm = ((a1 == 5) << 6) | (a1 & 7);
				// cannot directly address bp, so we do it with an offset of 0
				if (a1 == 5) {
					out->offsize = 8;
					out->offset = 0;
				} else if (a1 == 4) {
					// we also cannot directly address sp, so we add a sib byte with no index
					out->sib = 0x24;
				}
			} else {
				// only offset
				out->rm = 0x04;
				out->sib = 0b00100101;
				out->offset = std::stoi(tokens[0], 0, 0);
				out->offsize = 32;
				if (out->reloc.second == NONE && (int8_t)out->offset == out->offset) {
					out->offsize = 8;
					if (out->rm & 0x80)
						out->rm ^= 0xc0;
				}
			}
		} else {
			// must be reg + offset
			short a1 = reg_num(tokens[0]);
			if (a1 == -1) {
				error = "symbole « " + tokens[0] + " » non défini";
				return nullptr;
			}
			// size override
			if (reg_size(tokens[0]) == 32)
				out->prefix = 0x67;
			// rex prefix if necessary
			if (a1 >= 8)
				out->rex = 0x41;
			// modrm
			out->rm = 0x80 | (a1 & 7);
			if ((a1 & 7) == 4)
				// we cannot directly address sp, so we add a sib byte with no index
				out->sib = 0x24;
			// offset
			out->offset = std::stoi(tokens[1], 0, 0);
			out->offsize = 32;
			if (out->reloc.second == NONE && (int8_t)out->offset == out->offset) {
				out->offsize = 8;
				if (out->rm & 0x80)
					out->rm ^= 0xc0;
			}
		}
	} else {
		// sib
		short base;
		short index;
		short scale;
		int offset;
		bool force = false;
		if (ops[0] != '*') {
			base = reg_num(tokens[0]);
			tokens.erase(tokens.begin());
			ops.erase(ops.begin());
		} else
			base = -1;
		if (ops.size() && ops.back() != '*' && reg_size(tokens.back()) == -1) {
			offset = std::stoi(tokens.back(), 0, 0);
			if (out->reloc.second != NONE)
				force = true;
			tokens.pop_back();
			ops.pop_back();
		} else
			offset = 0;
		if (tokens.size() == 0) {
			index = -1;
			scale = 0;
		} else if (tokens.size() == 1) {
			index = reg_num(tokens[0]);
			scale = 1;
		} else if (tokens.size() == 2) {
			index = reg_num(tokens[0]);
			scale = std::stoi(tokens[1]);
		} else
			return nullptr;
		if (scale == 1)
			scale = 0;
		else if (scale == 2)
			scale = 1;
		else if (scale == 4)
			scale = 2;
		else if (scale == 8)
			scale = 3;
		else
			return nullptr;
		if (index == 4) {
			error = "erreur : impossible d'utiliser sp comme un index";
			return nullptr;
		}
		// size override
		if (reg_size(tokens[0]) == 32)
			out->prefix = 0x67;
		// rex prefix if necessary
		if (base >= 8 || index >= 8)
			out->rex = 0x40 | ((index >= 8) << 1) | (base >= 8);
		if ((base & 7) == 5)
			force = true;
		// modrm
		out->rm = 0x04 | ((offset || force) << 7);
		// 5 means no base
		if (base == -1) {
			base = 5;
			out->rm &= ~0xc0;
		}
		// 4 means no index
		if (index == -1) {
			index = 4;
			out->rm &= ~0xc0;
		}
		// sib
		out->sib = (scale << 6) | ((index & 7) << 3) | (base & 7);
		// offset
		if (offset || force) {
			out->offset = offset;
			out->offsize = 32;
			if (out->reloc.second == NONE && (int8_t)offset == offset) {
				out->offsize = 8;
				if (out->rm & 0x80)
					out->rm ^= 0xc0;
			}
		}
	}
	return out;
}

std::pair<unsigned long long, short> parse_imm(std::string s) {
	// if label, return label
	if (s[0] == '.')
		s = prev_label + s;
	if (s.ends_with(" wrt ..plt")) {
		if (!extern_labels_map.count(s.substr(0, s.size() - 10))) {
			error = "symbole « " + s.substr(0, s.size() - 10) + " » non défini";
			return {0, -1};
		}
		return {0, -4};
	}
	if (text_labels_map.count(s))
		return {text_labels_map.at(s), -3};
	if (extern_labels_map.count(s))
		return {extern_labels_map.at(s), -5};
	if (labels.count(s))
		return {0, -2};
	// if character, return character
	if (s[0] == '\'') {
		if (s[2] != '\'') {
			error = "charactère invalide";
			return {0, -1};
		}
		return {(unsigned char)s[1], 8};
	}
	// detect base
	int base = 0;
	if (s.starts_with("0x"))
		base = 16;
	else if (s.starts_with("0b"))
		base = 2;
	else
		base = 10;
	if (base != 10)
		s = s.substr(2);
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
	} catch (std::invalid_argument const &) {
		error = "symbole « " + s + " » non défini";
		return {0, -1};
	} catch (std::out_of_range const &) {
		error = "valeur d'immédiate trop grande";
		return {0, -1};
	}
}
