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

bool handle(const std::string &s, std::vector<std::string> args, const size_t linenum) {
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
			} else if (isdigit(token[0]) || (token[0] >= 'a' && token[0] <= 'f')) {
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
			std::vector<short> del;
			for (int j = 1; j < args.size(); j++) {
				if (isdigit(tokens[j][0]) || (tokens[j][0] >= 'a' && tokens[j][0] <= 'f'))
					del.push_back(j - 1);
			}
			for (auto d : del) {
				args.erase(args.begin() + d);
				types.erase(types.begin() + d);
				tokens.erase(tokens.begin() + d + 1);
			}
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
	std::vector<short> bestreloc;
	for (const auto &p : valid) {
		std::vector<short> reloc;
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
				short tmpreloc;
				std::deque<uint8_t> data = parse_mem(args[0], s1, tmpreloc);
				if (data.empty()) {
					if (error.empty())
						error = "invalid addressing mode";
					std::cerr << input_name << ":" << linenum << ": error: " << error << std::endl;
					return false;
				}
				if (data[0] == 0x67) {
					tmpreloc += 0x67;
					data.pop_front();
					tmpreloc -= tmpreloc != 0x7fff;
				}
				if ((data[0] & 0xf0) == 0x40) {
					tmpreloc += data[0] | (w << 3);
					data.pop_front();
					tmpreloc -= tmpreloc != 0x7fff;
				} else if (w)
					tmpreloc += 0x48;
				short reg = 0;
				for (size_t i = w; i < p.first.back().size(); i += 2) {
					if (p.first.back()[i] == '/') {
						reg = std::stoi(p.first.back().substr(i + 1, 1), nullptr, 16);
						break;
					}
					tmpreloc += std::stoi(p.first.back().substr(i, 2), nullptr, 16);
				}
				if (tmpreloc != 0x7fff && !(tmpreloc & 0x8000))
					reloc.push_back(0x4000 | tmpreloc);
				else if (tmpreloc != 0x7fff)
					reloc.push_back(0xa000 ^ tmpreloc);
				tmp += data.front() | (reg << 3);
				tmp.insert(tmp.end(), data.begin() + 1, data.end());
			} else if (p.first[1][0] == 'I') {
				auto a1 = parse_imm(args[0]);
				short s1 = _sizes[p.first[1][1] - 'A'];
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
					reloc.push_back(0x8000 | tmp.size());
				} else if (a1.second == -3) {
					reloc.push_back(0x4000 | tmp.size());
				} else if (a1.second == -4) {
					reloc.push_back(0x2000 | tmp.size());
				}
				for (int i = 0; i < s1; i += 8)
					tmp += (a1.first >> i) & 0xff;
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
					reg = {i, types[i - 1].second};
				else if (p.first[i][0] == 'M')
					mem = {i, types[i - 1].second};
				else if (p.first[i][0] == 'I')
					imm = {i, types[i - 1].second};
			}
			if (mem.first == -1) {
				short a1 = reg_num(args[reg.first - 1]);
				size_t i = p.first.back()[0] == 'w';
				if (i || (a1 & 8))
					tmp += 0x48 | (a1 >= 8);
				for (; i < p.first.back().size(); i += 2) {
					if (p.first.back()[i] == '/') {
						tmp += std::stoi(p.first.back().substr(i + 1, 1), nullptr, 16);
						break;
					}
					tmp += std::stoi(p.first.back().substr(i, 2), nullptr, 16);
				}
				tmp.back() += a1 & 7;
				auto a2 = parse_imm(args[imm.first - 1]);
				short s2 = _sizes[p.first[imm.first][1] - 'A'];
				for (int i = 0; i < s2; i += 8)
					tmp += (a2.first >> i) & 0xff;
			} else {
				short rex = 0;
				short rm = 0x7fff;
				short sib = 0x7fff;
				std::deque<uint8_t> other;
				if (mem.first != -1) {
					short s1 = mem.second;
					short tmpreloc;
					other = parse_mem(args[mem.first - 1], s1, tmpreloc);
					if (other.empty()) {
						if (error.empty())
							error = "invalid addressing mode";
						std::cerr << input_name << ":" << linenum << ": error: " << error << std::endl;
						return false;
					}
					if (other.front() == 0x67) {
						tmp += 0x67;
						other.pop_front();
					}
					if ((other.front() & 0xf0) == 0x40) {
						rex = other.front();
						other.pop_front();
					}
					if (!other.empty()) {
						rm = other.front();
						other.pop_front();
					}
					if (!other.empty()) {
						sib = other.front();
						other.pop_front();
					}
				}
				if (p.first.back()[0] == 'w')
					rex |= 0x48;
				if (reg.first != -1) {
					short a1 = reg_num(args[reg.first - 1]);
					if (a1 & 8)
						rex |= 0x40 | ((a1 & 8) >> 1);
					if (rm == 0x7fff)
						rm = 0xc0 | ((a1 & 7) << 3);
					else
						rm |= (a1 & 7) << 3;
				}
				if (rex != 0)
					tmp += rex;
				for (size_t i = p.first.back()[0] == 'w'; i < p.first.back().size(); i += 2) {
					if (p.first.back()[i] == '/') {
						rm |= std::stoi(p.first.back().substr(i + 1, 1), nullptr, 16) << 3;
						break;
					}
					tmp += std::stoi(p.first.back().substr(i, 2), nullptr, 16);
				}
				if (rm != 0x7fff)
					tmp += rm;
				if (sib != 0x7fff)
					tmp += sib;
				tmp.insert(tmp.end(), other.begin(), other.end());
				if (imm.first != -1) {
					auto a1 = parse_imm(args[imm.first - 1]);
					short s1 = _sizes[p.first[imm.first][1] - 'A'];
					if (a1.second == -1) {
						std::cerr << input_name << ":" << linenum << ": error: " << error << std::endl;
						return false;
					} else if (a1.second == -2) {
						reloc.push_back(0x8000 | tmp.size());
					} else if (a1.second == -3) {
						reloc.push_back(0x4000 | tmp.size());
					} else if (a1.second == -4) {
						reloc.push_back(0x2000 | tmp.size());
					}
					for (int i = 0; i < s1; i += 8)
						tmp += (a1.first >> i) & 0xff;
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
	for (auto reloc : bestreloc) {
		if (reloc & 0x4000)
			data_relocations.push_back(output_buffer.size() + (reloc & 0xff));
		else if (reloc & 0x2000)
			bss_relocations.push_back(output_buffer.size() + (reloc & 0xff));
		else
			text_relocations.push_back(output_buffer.size() + (reloc & 0xff));
	}
	for (size_t i = 0; i < best.size(); i++)
		output_buffer.push_back(best[i]);
	return true;
}
