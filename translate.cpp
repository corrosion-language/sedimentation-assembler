#include "translate.hpp"

std::string error = "";

// instruction handlers

bool mov(std::vector<std::string> &args) {
	if (args.size() != 2)
		return false;
	// check argument types
	enum op_type t1 = op_type(args[0]);
	enum op_type t2 = op_type(args[1]);
	if (t1 == REG && t2 == REG) {
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
	} else if (t1 == REG && t2 == MEM) {
		short s1 = reg_size(args[0]);
		short s2 = s1;
		short tmp = 0x7fff;
		std::vector<uint8_t> data = parse_mem(args[1], s2, tmp);
		if (data.empty())
			return false;
		if (s1 == 16) {
			output_buffer.push_back(0x66);
		}
		if (data[0] == 0x67) {
			output_buffer.push_back(0x67);
			data.erase(data.begin());
			tmp -= (tmp != 0x7fff);
		}
		short a1 = reg_num(args[0]);
		a1 += (args[0][1] == 'h') * 4;
		if ((data[0] & 0xf0) == 0x40) {
			output_buffer.push_back(data[0]);
			data.erase(data.begin());
			tmp -= (tmp != 0x7fff);
			output_buffer[output_buffer.size() - 1] |= ((s1 == 64) << 3) | ((a1 & 8) >> 1);
		} else if (s1 == 64 || (s1 == 8 && a1 >= 4 && args[0][1] != 'h'))
			output_buffer.push_back(0x40 | ((s1 == 64) << 3) | ((a1 & 8) >> 1));
		a1 &= 7;
		output_buffer.push_back(0x8b ^ (s1 == 8));
		if (tmp != 0x7fff && !(tmp & 0x8000))
			data_relocations.push_back(output_buffer.size() + tmp);
		else if (tmp != 0x7fff)
			bss_relocations.push_back(output_buffer.size() + (tmp & 0x7fff));
		output_buffer.push_back((a1 << 3) | data[0]);
		output_buffer.insert(output_buffer.end(), data.begin() + 1, data.end());
	} else if (t1 == MEM && t2 == REG) {
		short s2 = reg_size(args[1]);
		short s1 = s2;
		short tmp = 0x7fff;
		std::vector<uint8_t> data = parse_mem(args[0], s1, tmp);
		if (data.empty())
			return false;
		if (s1 == 16) {
			output_buffer.push_back(0x66);
		}
		if (data[0] == 0x67) {
			output_buffer.push_back(0x67);
			data.erase(data.begin());
			tmp -= (tmp != 0x7fff);
		}
		short a2 = reg_num(args[1]);
		a2 += (args[1][1] == 'h') * 4;
		if ((data[0] & 0xf0) == 0x40) {
			output_buffer.push_back(data[0]);
			data.erase(data.begin());
			tmp -= (tmp != 0x7fff);
			output_buffer[output_buffer.size() - 1] |= ((s1 == 64) << 3) | ((a2 & 8) >> 1);
		} else if (s1 == 64 || (s1 == 8 && a2 >= 4 && args[0][1] != 'h'))
			output_buffer.push_back(0x40 | ((s1 == 64) << 3) | ((a2 & 8) >> 1));
		a2 &= 7;
		output_buffer.push_back(0x89 ^ (s1 == 8));
		if (tmp != 0x7fff && !(tmp & 0x8000))
			data_relocations.push_back(output_buffer.size() + tmp);
		else if (tmp != 0x7fff)
			bss_relocations.push_back(output_buffer.size() + (tmp & 0x7fff));
		output_buffer.push_back((a2 << 3) | data[0]);
		output_buffer.insert(output_buffer.end(), data.begin() + 1, data.end());
	} else if (t1 == REG && t2 == IMM) {
		int s1 = reg_size(args[0]);
		int a1 = reg_num(args[0]);
		// detect base
		auto a2 = parse_imm(args[1]);
		short reloc = 0;
		if (a2.second == -1) {
			error = "invalid immediate value";
			return false;
		} else if (a2.second == -2 || a2.second > s1) {
			error = "overflow in immediate value";
			return false;
		} else if (a2.second == -3) {
			a2 = {a2.first, 32};
			reloc = 1;
		} else if (a2.second == -4) {
			a2 = {a2.first, 32};
			reloc = 2;
		}
		if (s1 < a2.second)
			return false;
		if (s1 == 64 && a2.second <= 32)
			s1 = 32;
		a1 += (args[0][1] == 'h') * 4;
		if (s1 == 8) {
			if (args[0][1] != 'h' && a1 > 4)
				output_buffer.push_back(0x40 | ((a1 & 8) >> 3));
			output_buffer.push_back(0xb0 + (a1 & 7));
			if (reloc == 1)
				data_relocations.push_back(output_buffer.size());
			else if (reloc == 2)
				bss_relocations.push_back(output_buffer.size());
			output_buffer.push_back(a2.first);
		} else if (s1 == 16) {
			output_buffer.push_back(0x66);
			if (a1 & 8)
				output_buffer.push_back(0x41);
			output_buffer.push_back(0xb8 + (a1 & 7));
			if (reloc == 1)
				data_relocations.push_back(output_buffer.size());
			else if (reloc == 2)
				data_relocations.push_back(output_buffer.size());
			output_buffer.push_back(a2.first & 0xff);
			output_buffer.push_back(a2.first >> 8);
		} else if (s1 == 32) {
			if (a1 & 8)
				output_buffer.push_back(0x41);
			output_buffer.push_back(0xb8 + (a1 & 7));
			if (reloc == 1)
				data_relocations.push_back(output_buffer.size());
			else if (reloc == 2)
				bss_relocations.push_back(output_buffer.size());
			output_buffer.push_back(a2.first & 0xff);
			output_buffer.push_back((a2.first >> 8) & 0xff);
			output_buffer.push_back((a2.first >> 16) & 0xff);
			output_buffer.push_back((a2.first >> 24) & 0xff);
		} else if (s1 == 64) {
			output_buffer.push_back(0x48 | ((a1 & 8) >> 3));
			output_buffer.push_back(0xb8 + (a1 & 7));
			if (reloc == 1)
				data_relocations.push_back(output_buffer.size());
			else if (reloc == 2)
				bss_relocations.push_back(output_buffer.size());
			output_buffer.push_back(a2.first & 0xff);
			output_buffer.push_back((a2.first >> 8) & 0xff);
			output_buffer.push_back((a2.first >> 16) & 0xff);
			output_buffer.push_back((a2.first >> 24) & 0xff);
			output_buffer.push_back((a2.first >> 32) & 0xff);
			output_buffer.push_back((a2.first >> 40) & 0xff);
			output_buffer.push_back((a2.first >> 48) & 0xff);
			output_buffer.push_back((a2.first >> 56) & 0xff);
		} else
			return false;
	} else if (t1 == MEM && t2 == IMM) {
		auto a2 = parse_imm(args[1]);
		short reloc = 0;
		if (a2.second == -1) {
			error = "invalid immediate value";
			return false;
		} else if (a2.second == -3) {
			a2 = {a2.first, 32};
			reloc = 1;
		} else if (a2.second == -4) {
			a2 = {a2.first, 32};
			reloc = 2;
		}
		short s1 = -1;
		short tmp = 0x7fff;
		std::vector<uint8_t> data = parse_mem(args[0], s1, tmp);
		if (s1 < a2.second)
			return false;
		if (data.empty())
			return false;
		if (s1 == 16) {
			output_buffer.push_back(0x66);
		}
		if (data[0] == 0x67) {
			output_buffer.push_back(0x67);
			data.erase(data.begin());
			tmp -= (tmp != 0x7fff);
		}
		if ((data[0] & 0xf0) == 0x40) {
			output_buffer.push_back(data[0]);
			data.erase(data.begin());
			tmp -= (tmp != 0x7fff);
			output_buffer[output_buffer.size() - 1] |= (s1 == 64) << 3;
		} else if (s1 == 64) {
			output_buffer.push_back(0x48);
		}
		output_buffer.push_back(0xc7 ^ (s1 == 8));
		if (tmp != 0x7fff && !(tmp & 0x8000))
			data_relocations.push_back(output_buffer.size() + tmp);
		else if (tmp != 0x7fff)
			bss_relocations.push_back(output_buffer.size() + (tmp & 0x7fff));
		output_buffer.insert(output_buffer.end(), data.begin(), data.end());
		if (reloc == 1)
			data_relocations.push_back(output_buffer.size());
		else if (reloc == 2)
			bss_relocations.push_back(output_buffer.size() + (tmp & 0x7fff));
		output_buffer.push_back(a2.first & 0xff);
		if (s1 >= 16)
			output_buffer.push_back((a2.first >> 8) & 0xff);
		if (s1 >= 32) {
			output_buffer.push_back((a2.first >> 16) & 0xff);
			output_buffer.push_back((a2.first >> 24) & 0xff);
		}
		if (s1 >= 64) {
			output_buffer.push_back((a2.first >> 32) & 0xff);
			output_buffer.push_back((a2.first >> 40) & 0xff);
			output_buffer.push_back((a2.first >> 48) & 0xff);
			output_buffer.push_back((a2.first >> 56) & 0xff);
		}
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

bool jmp(std::vector<std::string> &args) {
	if (args.size() != 1)
		return false;
	if (args[0][0] == '.')
		args[0] = prev_label + args[0];
	output_buffer.push_back(0xe9);
	text_relocations.push_back({output_buffer.size(), 1});
	uint32_t symid = text_labels_map.at(args[0]);
	output_buffer.push_back(symid & 0xff);
	output_buffer.push_back((symid >> 8) & 0xff);
	output_buffer.push_back((symid >> 16) & 0xff);
	output_buffer.push_back((symid >> 24) & 0xff);
	return true;
}

bool nop(std::vector<std::string> &args) {
	if (args.size() != 0)
		return false;
	output_buffer.push_back(0x90);
	return true;
}

bool inc(std::vector<std::string> &args) {
	if (args.size() != 1)
		return false;
	enum op_type t1 = op_type(args[0]);
	if (t1 == REG) {
		short s1 = reg_size(args[0]);
		short a1 = reg_num(args[0]);
		a1 += (args[0][1] == 'h') * 4;
		if (s1 == 8) {
			if (args[0][1] != 'h' && a1 > 4)
				output_buffer.push_back(0x40 | ((a1 & 8) >> 3));
			output_buffer.push_back(0xfe);
			output_buffer.push_back(0xc0 | (a1 & 7));
		} else if (s1 == 16) {
			output_buffer.push_back(0x66);
			if (a1 & 8)
				output_buffer.push_back(0x41);
			output_buffer.push_back(0xff);
			output_buffer.push_back(0xc0 | (a1 & 7));
		} else if (s1 == 32) {
			if (a1 & 8)
				output_buffer.push_back(0x41);
			output_buffer.push_back(0xff);
			output_buffer.push_back(0xc0 | (a1 & 7));
		} else if (s1 == 64) {
			output_buffer.push_back(0x48 | ((a1 & 8) >> 3));
			output_buffer.push_back(0xff);
			output_buffer.push_back(0xc0 | (a1 & 7));
		} else
			return false;
	} else if (t1 == MEM) {
		short s1 = -1;
		short tmp = 0x7fff;
		std::vector<uint8_t> data = parse_mem(args[0], s1, tmp);
		if (data.empty())
			return false;
		if (s1 == 16)
			output_buffer.push_back(0x66);
		if (data[0] == 0x67) {
			output_buffer.push_back(0x67);
			data.erase(data.begin());
			tmp -= (tmp != 0x7fff);
		}
		if ((data[0] & 0xf0) == 0x40) {
			output_buffer.push_back(data[0] | ((s1 == 64) << 3));
			data.erase(data.begin());
			tmp -= (tmp != 0x7fff);
		} else if (s1 == 64)
			output_buffer.push_back(0x48);
		output_buffer.push_back(0xff ^ (s1 == 8));
		if (tmp != 0x7fff && !(tmp & 0x8000))
			data_relocations.push_back(output_buffer.size() + tmp);
		else if (tmp != 0x7fff)
			bss_relocations.push_back(output_buffer.size() + (tmp & 0x7fff));
		output_buffer.insert(output_buffer.end(), data.begin(), data.end());
	} else {
		return false;
	}
	return true;
}

bool dec(std::vector<std::string> &args) {
	if (args.size() != 1)
		return false;
	enum op_type t1 = op_type(args[0]);
	if (t1 == REG) {
		short s1 = reg_size(args[0]);
		short a1 = reg_num(args[0]);
		a1 += (args[0][1] == 'h') * 4;
		if (s1 == 8) {
			if (args[0][1] != 'h' && a1 > 4)
				output_buffer.push_back(0x40 | ((a1 & 8) >> 3));
			output_buffer.push_back(0xfe);
			output_buffer.push_back(0xc8 | (a1 & 7));
		} else if (s1 == 16) {
			output_buffer.push_back(0x66);
			if (a1 & 8)
				output_buffer.push_back(0x41);
			output_buffer.push_back(0xff);
			output_buffer.push_back(0xc8 | (a1 & 7));
		} else if (s1 == 32) {
			if (a1 & 8)
				output_buffer.push_back(0x41);
			output_buffer.push_back(0xff);
			output_buffer.push_back(0xc8 | (a1 & 7));
		} else if (s1 == 64) {
			output_buffer.push_back(0x48 | ((a1 & 8) >> 3));
			output_buffer.push_back(0xff);
			output_buffer.push_back(0xc8 | (a1 & 7));
		} else
			return false;
	} else if (t1 == MEM) {
		short s1 = -1;
		short tmp = 0x7fff;
		std::vector<uint8_t> data = parse_mem(args[0], s1, tmp);
		if (data.empty())
			return false;
		if (s1 == 16)
			output_buffer.push_back(0x66);
		if (data[0] == 0x67) {
			output_buffer.push_back(0x67);
			data.erase(data.begin());
			tmp -= (tmp != 0x7fff);
		}
		if ((data[0] & 0xf0) == 0x40) {
			output_buffer.push_back(data[0] | ((s1 == 64) << 3));
			data.erase(data.begin());
			tmp -= (tmp != 0x7fff);
		} else if (s1 == 64)
			output_buffer.push_back(0x48);
		output_buffer.push_back(0xff ^ (s1 == 8));
		output_buffer.push_back(data[0] | 0x08);
		if (tmp != 0x7fff && !(tmp & 0x8000))
			data_relocations.push_back(output_buffer.size() + tmp);
		else if (tmp != 0x7fff)
			bss_relocations.push_back(output_buffer.size() + (tmp & 0x7fff));
		output_buffer.insert(output_buffer.end(), data.begin() + 1, data.end());
	} else {
		return false;
	}
	return true;
}

bool movzx(std::vector<std::string> &args) {
	if (args.size() != 2)
		return false;
	enum op_type t1 = op_type(args[0]);
	enum op_type t2 = op_type(args[1]);
	if (t1 != REG)
		return false;
	if (t2 == REG) {
		short s1 = reg_size(args[0]);
		short s2 = reg_size(args[1]);
		if (s1 <= s2)
			return false;
		short a1 = reg_num(args[0]);
		short a2 = reg_num(args[1]);
		a2 += (args[1][1] == 'h') * 4;
		if (s1 == 64 && args[1][1] == 'h')
			return false;
		if (s1 == 16)
			output_buffer.push_back(0x66);
		// rex byte if needed
		if (s1 == 64 || (s2 == 8 && a2 > 4 && args[1][1] != 'h'))
			output_buffer.push_back(0x40 | ((s1 == 64) << 3) | ((a1 & 8) >> 1) | ((a2 & 8) >> 3));
		output_buffer.push_back(0x0f);
		output_buffer.push_back(0xb6 | (s2 == 16));
		output_buffer.push_back(0xc0 | (a2 & 7) | ((a1 & 7) << 3));
	} else if (t2 == MEM) {
		short s1 = reg_size(args[0]);
		short s2 = -1;
		short tmp = 0x7fff;
		std::vector<uint8_t> data = parse_mem(args[1], s2, tmp);
		if (data.empty())
			return false;
		if (s1 <= s2)
			return false;
		if (s1 == 16)
			output_buffer.push_back(0x66);
		if (data[0] == 0x67) {
			output_buffer.push_back(0x67);
			data.erase(data.begin());
			tmp -= (tmp != 0x7fff);
		}
		short a1 = reg_num(args[0]);
		if ((data[0] & 0xf0) == 0x40) {
			output_buffer.push_back(data[0] | ((a1 & 8) >> 1) | ((s1 == 64) << 3));
			data.erase(data.begin());
			tmp -= (tmp != 0x7fff);
		} else if (s1 == 64)
			output_buffer.push_back(0x48);
		output_buffer.push_back(0x0f);
		output_buffer.push_back(0xb6 | (s2 == 16));
		if (tmp != 0x7fff && !(tmp & 0x8000))
			data_relocations.push_back(output_buffer.size() + tmp);
		else if (tmp != 0x7fff)
			bss_relocations.push_back(output_buffer.size() + (tmp & 0x7fff));
		output_buffer.push_back(((a1 & 7) << 3) | data[0]);
		output_buffer.insert(output_buffer.end(), data.begin() + 1, data.end());
	} else {
		return false;
	}
	return true;
}

const uint8_t adc_optable[6] = {0x15, 0x81, 0x83, 0x11, 0x13, 0x10};
const uint8_t add_optable[6] = {0x05, 0x81, 0x83, 0x01, 0x03, 0x00};
const uint8_t sub_optable[6] = {0x2d, 0x81, 0x83, 0x29, 0x2b, 0x28};
const uint8_t cmp_optable[6] = {0x3d, 0x81, 0x83, 0x39, 0x3b, 0x38};
const uint8_t and_optable[6] = {0x25, 0x81, 0x83, 0x21, 0x23, 0x20};
const uint8_t or_optable[6] = {0x0d, 0x81, 0x83, 0x09, 0x0b, 0x08};
const uint8_t xor_optable[6] = {0x35, 0x81, 0x83, 0x31, 0x33, 0x30};

bool _arith(std::vector<std::string> &args, const uint8_t opcodes[6]) {
	if (args.size() != 2)
		return false;
	enum op_type t1 = op_type(args[0]);
	enum op_type t2 = op_type(args[1]);
	short reloc = 0;
	if (t1 == REG && t2 == IMM) {
		int s1 = reg_size(args[0]);
		int a1 = reg_num(args[0]);
		auto a2 = parse_imm(args[1]);
		if (a2.second == -1 || a2.second == 64) {
			error = "invalid immediate value";
			return false;
		} else if (a2.second == -2 || a2.second > s1) {
			error = "overflow in immediate value";
			return false;
		} else if (a2.second == -3) {
			a2 = {a2.first, 32};
			reloc = 1;
		} else if (a2.second == -4) {
			a2 = {a2.first, 32};
			reloc = 2;
		}
		if (s1 < a2.second)
			return false;
		a1 += (args[0][1] == 'h') * 4;
		if (a1 == 0) {
			if (s1 == 16)
				output_buffer.push_back(0x66);
			if (s1 == 64)
				output_buffer.push_back(0x48);
			output_buffer.push_back(opcodes[0] ^ (s1 == 8));
			if (reloc == 1)
				data_relocations.push_back(output_buffer.size());
			else if (reloc == 2)
				bss_relocations.push_back(output_buffer.size());
			output_buffer.push_back(a2.first & 0xff);
			if (s1 >= 16)
				output_buffer.push_back((a2.first >> 8) & 0xff);
			if (s1 >= 32) {
				output_buffer.push_back((a2.first >> 16) & 0xff);
				output_buffer.push_back((a2.first >> 24) & 0xff);
			}
		} else {
			if (s1 == 16)
				output_buffer.push_back(0x66);
			if (s1 == 64 || (a1 >= 4 && args[0][1] != 'h'))
				output_buffer.push_back(0x40 | ((s1 == 64) << 3) | ((a1 & 8) >> 3));
			if (a2.second == 8 && s1 != 8)
				output_buffer.push_back(opcodes[2]);
			else
				output_buffer.push_back(opcodes[1] ^ (s1 == 8));
			output_buffer.push_back(0xc0 | opcodes[5] | (a1 & 7));
			if (reloc == 1)
				data_relocations.push_back(output_buffer.size());
			else if (reloc == 2)
				bss_relocations.push_back(output_buffer.size());
			output_buffer.push_back(a2.first & 0xff);
			if (a2.second != 8 && s1 >= 16)
				output_buffer.push_back((a2.first >> 8) & 0xff);
			if (a2.second != 8 && s1 >= 32) {
				output_buffer.push_back((a2.first >> 16) & 0xff);
				output_buffer.push_back((a2.first >> 24) & 0xff);
			}
		}
	} else if (t1 == MEM && t2 == IMM) {
		auto a2 = parse_imm(args[1]);
		short reloc = 0;
		if (a2.second == -1) {
			error = "invalid immediate value";
			return false;
		} else if (a2.second == -3) {
			a2 = {a2.first, 32};
			reloc = 1;
		} else if (a2.second == -4) {
			a2 = {a2.first, 32};
			reloc = 2;
		}
		short s1 = -1;
		short tmp = 0x7fff;
		std::vector<uint8_t> data = parse_mem(args[0], s1, tmp);
		if (s1 < a2.second)
			return false;
		if (data.empty())
			return false;
		if (s1 == 16)
			output_buffer.push_back(0x66);
		if (data[0] == 0x67) {
			output_buffer.push_back(0x67);
			data.erase(data.begin());
			tmp -= (tmp != 0x7fff);
		}
		if ((data[0] & 0xf0) == 0x40) {
			output_buffer.push_back(data[0] | ((s1 == 64) << 3));
			data.erase(data.begin());
			tmp -= (tmp != 0x7fff);
		} else if (s1 == 64)
			output_buffer.push_back(0x48);
		if (a2.second == 8 && s1 != 8)
			output_buffer.push_back(opcodes[2]);
		else
			output_buffer.push_back(opcodes[1] ^ (s1 == 8));
		if (tmp != 0x7fff && !(tmp & 0x8000))
			data_relocations.push_back(output_buffer.size() + tmp);
		else if (tmp != 0x7fff)
			bss_relocations.push_back(output_buffer.size() + tmp);
		output_buffer.push_back(opcodes[5] | data[0]);
		output_buffer.insert(output_buffer.end(), data.begin() + 1, data.end());
		if (reloc == 1)
			data_relocations.push_back(output_buffer.size());
		else if (reloc == 2)
			bss_relocations.push_back(output_buffer.size());
		output_buffer.push_back(a2.first & 0xff);
		if (a2.second != 8 && s1 >= 16)
			output_buffer.push_back((a2.first >> 8) & 0xff);
		if (a2.second != 8 && s1 >= 32) {
			output_buffer.push_back((a2.first >> 16) & 0xff);
			output_buffer.push_back((a2.first >> 24) & 0xff);
		}
	} else if (t1 == REG && t2 == REG) {
		short s1 = reg_size(args[0]);
		short s2 = reg_size(args[1]);
		if (s1 != s2)
			return false;
		short a1 = reg_num(args[0]);
		short a2 = reg_num(args[1]);
		a1 += (args[0][1] == 'h') * 4;
		a2 += (args[1][1] == 'h') * 4;
		if (s1 == 16)
			output_buffer.push_back(0x66);
		if (s1 == 64 || (s1 == 8 && a1 >= 4 && args[0][1] != 'h'))
			output_buffer.push_back(0x40 | ((s1 == 64) << 3) | ((a1 & 8) >> 1) | ((a2 & 8) >> 3));
		output_buffer.push_back(opcodes[4] ^ (s1 == 8));
		output_buffer.push_back(0xc0 | ((a1 & 7) << 3) | (a2 & 7));
	} else if (t1 == MEM && t2 == REG) {
		short s2 = reg_size(args[1]);
		short s1 = s2;
		short tmp = 0x7fff;
		std::vector<uint8_t> data = parse_mem(args[0], s1, tmp);
		if (data.empty())
			return false;
		if (s1 == 16)
			output_buffer.push_back(0x66);
		if (data[0] == 0x67) {
			output_buffer.push_back(0x67);
			data.erase(data.begin());
			tmp -= (tmp != 0x7fff);
		}
		short a2 = reg_num(args[1]);
		a2 += (args[1][1] == 'h') * 4;
		if ((data[0] & 0xf0) == 0x40) {
			output_buffer.push_back(data[0] | ((s1 == 64) << 3) | ((a2 & 8) >> 1));
			data.erase(data.begin());
			tmp -= (tmp != 0x7fff);
		} else if (s1 == 64 || (s1 == 8 && a2 >= 4 && args[0][1] != 'h'))
			output_buffer.push_back(0x40 | ((s1 == 64) << 3) | ((a2 & 8) >> 1));
		output_buffer.push_back(opcodes[3] ^ (s1 == 8));
		if (tmp != 0x7fff && !(tmp & 0x8000))
			data_relocations.push_back(output_buffer.size() + tmp);
		else if (tmp != 0x7fff)
			bss_relocations.push_back(output_buffer.size() + (tmp & 0x7fff));
		output_buffer.push_back(((a2 & 7) << 3) | data[0]);
		output_buffer.insert(output_buffer.end(), data.begin() + 1, data.end());
	} else if (t1 == REG && t2 == MEM) {
		short s1 = reg_size(args[0]);
		short s2 = s1;
		short tmp = 0x7fff;
		std::vector<uint8_t> data = parse_mem(args[1], s2, tmp);
		if (data.empty())
			return false;
		if (s1 == 16)
			output_buffer.push_back(0x66);
		if (data[0] == 0x67) {
			output_buffer.push_back(0x67);
			data.erase(data.begin());
			tmp -= (tmp != 0x7fff);
		}
		short a1 = reg_num(args[0]);
		a1 += (args[0][1] == 'h') * 4;
		if ((data[0] & 0xf0) == 0x40) {
			output_buffer.push_back(data[0] | ((s1 == 64) << 3) | ((a1 & 8) >> 1));
			data.erase(data.begin());
			tmp -= (tmp != 0x7fff);
		} else if (s1 == 64 || (s1 == 8 && a1 >= 4 && args[0][1] != 'h'))
			output_buffer.push_back(0x40 | ((s1 == 64) << 3) | ((a1 & 8) >> 1));
		output_buffer.push_back(opcodes[4] ^ (s1 == 8));
		if (tmp != 0x7fff && !(tmp & 0x8000))
			data_relocations.push_back(output_buffer.size() + tmp);
		else if (tmp != 0x7fff)
			bss_relocations.push_back(output_buffer.size() + (tmp & 0x7fff));
		output_buffer.push_back(((a1 & 7) << 3) | data[0]);
		output_buffer.insert(output_buffer.end(), data.begin() + 1, data.end());
	} else {
		return false;
	}
	return true;
}

bool adc(std::vector<std::string> &args) { return _arith(args, adc_optable); }
bool add(std::vector<std::string> &args) { return _arith(args, add_optable); }
bool sub(std::vector<std::string> &args) { return _arith(args, sub_optable); }
bool cmp(std::vector<std::string> &args) { return _arith(args, cmp_optable); }
bool _and(std::vector<std::string> &args) { return _arith(args, and_optable); }
bool _or(std::vector<std::string> &args) { return _arith(args, or_optable); }
bool _xor(std::vector<std::string> &args) { return _arith(args, xor_optable); }

// instruction, handler
const std::unordered_map<std::string, handler> _handlers{
	{"mov", mov}, {"syscall", syscall}, {"jmp", jmp}, {"nop", nop}, {"inc", inc},  {"dec", dec}, {"movzx", movzx},
	{"adc", adc}, {"add", add},			{"sub", sub}, {"cmp", cmp}, {"and", _and}, {"or", _or},	 {"xor", _xor},
};

bool handle(std::string &s, std::vector<std::string> &args, size_t linenum) {
	if (_handlers.find(s) == _handlers.end()) {
		std::cerr << input_name << ':' << linenum << ": error: unknown instruction " << s << std::endl;
		return false;
	}
	error = "";
	if (!_handlers.at(s)(args) || !error.empty()) {
		if (error.empty())
			error = "invalid combination of opcode and operands";
		std::cerr << input_name << ':' << linenum << ": error: " << error << std::endl;
		return false;
	}
	return true;
}
