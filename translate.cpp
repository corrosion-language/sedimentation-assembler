#include "translate.hpp"
#include <sys/mman.h>

std::string error = "";

bool inited = false;
size_t map_size;
char *map;

void init() {
	inited = true;
	FILE *tmp = fopen("instr.dat", "r");
	fseek(tmp, 0, SEEK_END);
	map_size = ftell(tmp);
	map = (char *)mmap(NULL, map_size, PROT_READ, MAP_PRIVATE, fileno(tmp), 0);
	fclose(tmp);
}

bool handle(const std::string &s, const std::vector<std::string> &args, const size_t linenum) {
	if (!inited)
		init();
	std::vector<std::string> matches;
	char *l = map;
	char *r = std::find(l, map + map_size, '\n');
	do {
		if (std::string(l, l + s.size()) == s)
			matches.push_back(std::string(l, r));
		l = r + 1;
		r = std::find(l, map + map_size, '\n');
	} while (r < map + map_size);
	if (matches.empty()) {
		std::cerr << input_name << ":" << linenum << ": error: unknown instruction " << s << std::endl;
		return false;
	}
	std::vector<std::pair<enum op_type, short>> types;
	for (const std::string &arg : args) {
		enum op_type type = op_type(arg);
		if (type == REG) {
			types.push_back({REG, reg_size(arg)});
		} else if (type == MEM) {
			types.push_back({MEM, mem_size(arg)});
		} else if (type == IMM) {
			auto tmp = parse_imm(arg);
			if (tmp.second == -1) {
				std::cerr << input_name << ":" << linenum << ": error: " << error << std::endl;
				return false;
			} else if (tmp.second <= -2) {
				types.push_back({IMM, 32});
			} else {
				types.push_back({IMM, tmp.second});
			}
		} else {
			return false;
		}
	}
	std::vector<std::pair<std::vector<std::string>, short>> valid;
	for (size_t i = 0; i < matches.size(); i++) {
		if (args.size() > 2)
			continue;
		std::string &line = matches[i];
		std::vector<std::string> tokens;
		size_t l = 0;
		size_t r = line.find(' ');
		while (r != std::string::npos) {
			tokens.push_back(line.substr(l, r - l));
			l = r + 1;
			r = line.find(' ', l);
		}
		tokens.push_back(line.substr(l));
		if (tokens.size() != args.size() + 2)
			continue;

		bool matched = true;
		short size = 0;
		for (size_t j = 0; j < args.size(); j++) {
			std::string &token = tokens[j + 1];
			if (token[0] == 'R') {
				if (types[j].first != REG) {
					matched = false;
					break;
				}
				if (types[j].second != _sizes[token[1] - 'A']) {
					matched = false;
					break;
				}
				size += _sizes[token[1] - 'A'];
			} else if (token[0] == 'M') {
				if (types[j].first != MEM && types[j].first != REG) {
					matched = false;
					break;
				}
				if (token[1] == '*') {
					if (types[j].first != MEM) {
						matched = false;
						break;
					} else {
						continue;
					}
				}
				if (types[j].second != -1 && types[j].second != _sizes[token[1] - 'A']) {
					matched = false;
					break;
				}
				size += _sizes[token[1] - 'A'];
			} else if (token[0] == 'I') {
				if (types[j].first != IMM) {
					matched = false;
					break;
				}
				if (types[j].second > _sizes[token[1] - 'A']) {
					matched = false;
					break;
				}
			} else if (token[0] == '0') {
				if (reg_num(args[j]) != std::stoi(token.substr(0, 1), nullptr, 16)) {
					matched = false;
					break;
				}
				if (types[j].second != _sizes[token[1] - 'A']) {
					matched = false;
					break;
				}
				size += _sizes[token[1] - 'A'];
			} else {
				std::cerr << "Error: misconfigured instruction set" << std::endl;
				return false;
			}
		}
		if (matched) {
			valid.push_back({tokens, size});
		}
	}
	if (valid.empty()) {
		std::cerr << input_name << ":" << linenum << ": error: invalid combination of opcode and operands" << std::endl;
		return false;
	}
	for (size_t i = 1; i < valid.size(); i++) {
		if (valid[i].second != valid[i - 1].second) {
			std::cerr << input_name << ":" << linenum << ": error: operation size not specified" << std::endl;
			return false;
		}
	}
	std::string best = "";
	size_t bestlen = -1;
	short bestreloc = 0x7fff;
	for (const auto &p : valid) {
		short reloc = 0x7fff;
		std::string tmp = "";
		if (types.size() == 0) {
			for (size_t i = 0; i < p.first.back().size(); i += 2)
				tmp += std::stoi(p.first.back().substr(i, 2), nullptr, 16);
		} else if (types.size() == 1) {
			if (types[0].second == 16)
				tmp += 0x66;
			if (types[0].first == REG) {
				short a1 = reg_num(args[0]);
				a1 += (args[0][1] == 'h') * 4;
				size_t i = p.first.back()[0] == 'w';
				if (args[0][1] == 'h' && i) {
					std::cerr << input_name << ":" << linenum << ": error: invalid combination of opcode and operands" << std::endl;
					return false;
				}
				if (i || (types[0].second == 8 && args[0][1] != 'h' && a1 >= 4) || a1 >= 8)
					tmp += 0x40 | (i << 3) | (a1 & 8);
				short reg = 0;
				for (; i < p.first.back().size(); i += 2) {
					if (p.first.back()[i] == '/') {
						reg = std::stoi(p.first.back().substr(i + 1, 1), nullptr, 16);
						break;
					}
					tmp += std::stoi(p.first.back().substr(i, 2), nullptr, 16);
				}
				if (p.first[1][0] == 'R')
					tmp.back() += a1;
				else
					tmp += 0xc0 | a1;
				if (reg != -1)
					tmp.back() |= reg << 3;
			} else if (p.first[1][0] == 'M') {
				short s1 = _sizes[p.first[1][1] - 'A'];
				size_t w = p.first.back()[0] == 'w';
				std::deque<uint8_t> data = parse_mem(args[0], s1, reloc);
				if (data.empty()) {
					if (error.empty())
						error = "invalid addressing mode";
					std::cerr << input_name << ":" << linenum << ": error: " << error << std::endl;
					return false;
				}
				if (data[0] == 0x67) {
					tmp += 0x67;
					data.pop_front();
					reloc -= reloc != 0x7fff;
				}
				if ((data[0] & 0xf0) == 0x40) {
					tmp += data[0] | (w << 3);
					data.pop_front();
					reloc -= reloc != 0x7fff;
				} else if (w)
					tmp += 0x48;
				short reg = 0;
				for (size_t i = w; i < p.first.back().size(); i += 2) {
					if (p.first.back()[i] == '/') {
						reg = std::stoi(p.first.back().substr(i + 1, 1), nullptr, 16);
						break;
					}
					tmp += std::stoi(p.first.back().substr(i, 2), nullptr, 16);
				}
				if (reloc != 0x7fff && !(reloc & 0x8000))
					reloc |= 0x4000;
				else if (reloc != 0x7fff)
					reloc ^= 0xa000;
				tmp += data.front() | (reg << 3);
				tmp.insert(tmp.end(), data.begin() + 1, data.end());
			} else if (p.first[1][0] == 'I') {
				auto a1 = parse_imm(args[0]);
				short s1 = p.first[1][1] - 'A' + 1;
				size_t i = p.first.back()[0] == 'w';
				if (i)
					tmp += 0x48;
				for (; i < p.first.back().size(); i += 2) {
					if (p.first.back()[i] == '/') {
						tmp += std::stoi(p.first.back().substr(i + 1, 1), nullptr, 16);
						break;
					}
					tmp += std::stoi(p.first.back().substr(i, 2), nullptr, 16);
				}
				if (a1.second == -1) {
					std::cerr << input_name << ":" << linenum << ": error: " << error << std::endl;
					return false;
				} else if (a1.second == -2) {
					reloc = 0x8000 | tmp.size();
				} else if (a1.second == -3) {
					reloc = 0x4000 | tmp.size();
				} else if (a1.second == -4) {
					reloc = 0x2000 | tmp.size();
				}
				for (int i = 0; i < s1; i++)
					tmp += (a1.first >> (i * 8)) & 0xff;
			}
		} else if (types.size() >= 2) {
			if ((types[0].first != IMM && types[0].second == 16) || (types[1].first != IMM && types[1].second == 16))
				tmp += 0x66;
			// index, size
			std::pair<short, short> reg{-1, -1};
			std::pair<short, short> mem{-1, -1};
			std::pair<short, short> imm{-1, -1};
			for (size_t i = 1; i <= args.size(); i++) {
				if (p.first[i][0] == 'R')
					reg = {i, types[i].second};
				else if (types[i].first == MEM)
					mem = {i, types[i].second};
				else if (types[i].first == IMM)
					imm = {i, types[i].second};
			}
			std::deque<uint8_t> data;
			if (mem.first != -1) {
				short s1 = _sizes[p.first[mem.first][1] - 'A'];
				data = parse_mem(args[mem.first - 1], s1, reloc);
				if (data.empty()) {
					if (error.empty())
						error = "invalid addressing mode";
					std::cerr << input_name << ":" << linenum << ": error: " << error << std::endl;
					return false;
				}
			}
		}
		if (tmp.size() < bestlen) {
			best = tmp;
			bestlen = tmp.size();
			bestreloc = reloc;
		}
		if (bestlen == 1)
			break;
	}
	if (bestreloc != 0x7fff) {
		if (bestreloc & 0x4000)
			data_relocations.push_back(output_buffer.size() + (bestreloc & 0xff));
		else if (bestreloc & 0x2000)
			bss_relocations.push_back(output_buffer.size() + (bestreloc & 0xff));
		else
			text_relocations.push_back(output_buffer.size() + (bestreloc & 0xff));
	}
	for (size_t i = 0; i < best.size(); i++)
		output_buffer.push_back(best[i]);
	return true;
}
