#include "translate.hpp"
#include "instr.dat"
#include "vex.hpp"
#include <sys/mman.h>

void handle(std::string s, std::vector<std::string> args, const size_t linenum, size_t instr_cnt) {
	error = "";
	bool prefix = false;
	// handle prefixes (lock, repne, repe)
	while (true) {
		if (s == "lock") {
			output_buffer.push_back(0xf0);
			prefix = true;
		} else if (s == "repne" || s == "repnz" || s == "bnd") {
			output_buffer.push_back(0xf2);
			prefix = true;
		} else if (s == "rep" || s == "repe" || s == "repz") {
			output_buffer.push_back(0xf3);
			prefix = true;
		} else
			break;
		size_t i = args.front().find(' ');
		s = args.front().substr(0, i);
		args.front() = args.front().substr(i + 1);
	}
	char *l = map;
	char *r = map + map_size - 1;
	// go to the start of the section
	while (r[-1] != '\n' || r[-2] != '\n')
		r--;
	// binary search for the section that matches
	while (l < r && l < map + map_size - 2 && r > map + 2) {
		char *m = l + (r - l) / 2;
		while (m >= map + 2 && (m[-1] != '\n' || m[-2] != '\n'))
			m--;
		if (strncmp(m, s.c_str(), s.size()) == 0 && m[s.size()] == ' ') {
			l = m;
			break;
		} else if (strncmp(m, s.c_str(), s.size()) < 0) {
			l = m + 6;
			while (l <= map + map_size - 2 && (l[-1] != '\n' || l[-2] != '\n'))
				l++;
		} else {
			r = m - 6;
			while (r >= map + 2 && (r[-1] != '\n' || r[-2] != '\n'))
				r--;
		}
	}
	// store all lines that match
	while ((strncmp(l, s.c_str(), s.size()) != 0 || l[s.size()] != ' ') && l < map + map_size)
		l++;
	if (l >= map + map_size - 1 || *(l - 1) != '\n') {
		handle_vex(s, args, linenum, prefix);
		return;
	}
	r = std::find(l + 1, map + map_size, '\n');
	std::vector<std::string> matches;
	while (*l != '\n' && r != map + map_size) {
		matches.emplace_back(std::string(l, r));
		l = r + 1;
		r = std::find(l, map + map_size, '\n');
	}
	std::vector<std::pair<enum op_type, short>> types;
	for (const std::string &arg : args) {
		enum op_type type = get_optype(arg);
		if (type == REG) {
			types.emplace_back(REG, reg_size(arg));
		} else if (type == MEM) {
			types.emplace_back(MEM, mem_size(arg));
		} else if (type == IMM) {
			auto tmp = parse_imm(arg);
			if (tmp.second == -1) {
				cerr(linenum, error);
			} else if (tmp.second == -3) {
				if (reloc_table.count(text_labels[tmp.first])) {
					int32_t off = reloc_table.at(text_labels.at(tmp.first)) - output_buffer.size() - 5;
					if ((int8_t)off == off) {
						types.emplace_back(IMM, 8);
					} else {
						types.emplace_back(IMM, 32);
						tmp.first = 0;
					}
				} else {
					if (text_labels_instr[tmp.first] - instr_cnt <= 9) {
						types.emplace_back(IMM, 8);
					} else {
						types.emplace_back(IMM, 32);
						tmp.first = 0;
					}
				}
			} else if (tmp.second == -2 || tmp.second == -4) {
				types.emplace_back(IMM, 32);
			} else {
				types.emplace_back(IMM, tmp.second);
			}
		} else {
			cerr(linenum, "opérande invalide « " + arg + " »");
		}
	}
	std::vector<std::pair<std::vector<std::string>, short>> valid;
	for (size_t i = 0; i < matches.size(); i++) {
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
			} else if (token[0] == 'L') {
				if (std::stoi(args[j]) != std::stoi(token.substr(1))) {
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
				std::cerr << "Erreur : ensemble d'instructions mal configurée" << std::endl;
			}
		}
		if (matched) {
			for (size_t j = 0; j < args.size(); j++) {
				if (types[j].second == -1 && tokens[j + 1][1] == '*')
					types[j].second = 8;
			}
			std::vector<short> del;
			for (size_t j = 1; j <= args.size(); j++) {
				if (isdigit(tokens[j][0]) || (tokens[j][0] >= 'a' && tokens[j][0] <= 'f'))
					del.push_back(j - 1);
				else if (tokens[j][0] == 'L')
					del.push_back(j - 1);
			}
			for (auto d : del) {
				args.erase(args.begin() + d);
				types.erase(types.begin() + d);
				tokens.erase(tokens.begin() + d + 1);
			}
			valid.emplace_back(tokens, size);
		}
	}
	if (valid.empty())
		cerr(linenum, "combination d'opcode et des opérandes invalide");
	if (valid.size() > 1) {
		for (size_t i = 0; i < args.size(); i++) {
			if (types[i].second == -1)
				cerr(linenum, "taille d'opération non spécifiée");
		}
	} else {
		for (size_t i = 0; i < args.size(); i++) {
			if (types[i].first == MEM)
				types[i].second = _sizes[valid[0].first[i + 1][1] - 'A'];
		}
	}
	if (types.size() == 2 && types[0].first == REG && types[1].first == REG) {
		types[1].first = MEM;
		for (auto &p : valid)
			p.first[2].front() = 'M';
	}
	std::string best = "";
	size_t bestlen = -1;
	std::vector<reloc_entry> bestreloc;
	for (auto &p : valid) {
		std::vector<reloc_entry> reloc;
		std::string tmp = "";
		if (p.first.back()[0] == 'p') {
			tmp += std::stoi(p.first.back().substr(1, 2), nullptr, 16);
			p.first.back() = p.first.back().substr(3);
		}
		if (types.size() == 0) {
			if (p.first.back()[0] == 'w') {
				tmp += 0x48;
				p.first.back() = p.first.back().substr(1);
			}
			for (size_t i = 0; i < p.first.back().size(); i += 2)
				tmp += std::stoi(p.first.back().substr(i, 2), nullptr, 16);
		} else if (types.size() == 1) {
			if (types[0].second == 16)
				tmp += 0x66;
			if (types[0].first == REG) {
				short a1 = reg_num(args[0]);
				a1 += (args[0][1] == 'h') * 4;
				size_t i = p.first.back()[0] == 'w';
				if (args[0][1] == 'h' && i)
					cerr(linenum, "impossible d'utiliser un haut-demi registre avec une prefixe REX");
				if ((i && s != "push" && s != "pop") || (types[0].second == 8 && args[0][1] != 'h' && a1 >= 4) || a1 >= 8)
					tmp += 0x40 | (i << 3) | (a1 >= 8);
				short reg = 0;
				for (; i < p.first.back().size(); i += 2) {
					if (p.first.back()[i] == '/') {
						reg = std::stoi(p.first.back().substr(i + 1, 1));
						break;
					}
					tmp += std::stoi(p.first.back().substr(i, 2), nullptr, 16);
				}
				if (p.first[1][0] == 'R')
					tmp.back() += a1 & 7;
				else
					tmp += 0xc0 | (a1 & 7);
				if (reg != -1)
					tmp.back() |= reg << 3;
			} else if (p.first[1][0] == 'M') {
				short s1 = _sizes[p.first[1][1] - 'A'];
				size_t w = p.first.back()[0] == 'w';
				mem_output *data = parse_mem(args[0], s1);
				if (data == nullptr) {
					if (error.empty())
						error = "mode d'adressage invalide";
					cerr(linenum, error);
				}
				if (data->prefix)
					tmp += data->prefix;
				if (data->rex)
					tmp += data->rex;
				else if (w)
					tmp += 0x48;
				short reg = 0;
				for (size_t i = w; i < p.first.back().size(); i += 2) {
					if (p.first.back()[i] == '/') {
						reg = std::stoi(p.first.back().substr(i + 1, 1));
						break;
					}
					tmp += std::stoi(p.first.back().substr(i, 2), nullptr, 16);
				}
				tmp += (uint8_t)data->rm | (reg << 3);
				if (data->sib != 0x7fff)
					tmp += data->sib;

				if (data->reloc.second != NONE) {
					reloc.emplace_back(output_buffer.size() + tmp.size(), data->offset, data->reloc.second, data->reloc.first, 32);
					data->offset = 0;
				}

				for (int i = 0; i < data->offsize; i += 8)
					tmp += (data->offset >> i) & 0xff;

				delete data;
			} else if (p.first[1][0] == 'I') {
				auto a1 = parse_imm(args[0]);
				size_t i = p.first.back()[0] == 'w';
				if (i)
					tmp += 0x48;
				for (; i < p.first.back().size(); i += 2) {
					if (p.first.back()[i] == '/') {
						tmp += std::stoi(p.first.back().substr(i + 1, 1)) << 3;
						break;
					}
					tmp += std::stoi(p.first.back().substr(i, 2), nullptr, 16);
				}
				if (a1.second == -1) {
					cerr(linenum, error);
				} else if (a1.second == -2) {
					reloc.emplace_back(output_buffer.size() + tmp.size(), 0, ABS, args[0], 32);
				} else if (a1.second == -3) {
					if (reloc_table.count(text_labels[a1.first])) {
						int32_t off = reloc_table.at(text_labels.at(a1.first)) - output_buffer.size() - 1 - _sizes[p.first[1][1] - 'A'] / 8;
						if ((int8_t)off == off) {
							a1.first = off;
						} else {
							reloc.emplace_back(output_buffer.size() + tmp.size(), 0, REL, text_labels[a1.first], 32);
							a1.first = 0;
						}
					} else {
						if (text_labels_instr[a1.first] - instr_cnt <= 9) {
							reloc.emplace_back(output_buffer.size() + tmp.size(), 0, REL, text_labels[a1.first], 8);
							a1.first = 0;
						} else {
							reloc.emplace_back(output_buffer.size() + tmp.size(), 0, REL, text_labels[a1.first], 32);
							a1.first = 0;
						}
					}
				} else if (a1.second == -4) {
					reloc.emplace_back(output_buffer.size() + tmp.size(), 0, PLT, args[0].substr(0, args[0].size() - 10), 32);
				} else if (a1.second == -5) {
					reloc.emplace_back(output_buffer.size() + tmp.size(), 0, REL, extern_labels[a1.first], 32);
					a1.first = 0;
				}
				for (int i = 0; i < _sizes[p.first[1][1] - 'A']; i += 8)
					tmp += (a1.first >> i) & 0xff;
			}
		} else if (types.size() >= 2) {
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
				if (args[reg.first - 1][1] == 'h' && i)
					cerr(linenum, "impossible d'utiliser un haut-demi registre avec une prefixe REX");
				if (i || (a1 & 8))
					tmp += 0x40 | (i << 3) | (a1 >= 8);
				int reg = 0x7fff;
				for (; i < p.first.back().size(); i += 2) {
					if (p.first.back()[i] == '/') {
						reg = std::stoi(p.first.back().substr(i + 1, 1));
						break;
					}
					tmp += std::stoi(p.first.back().substr(i, 2), nullptr, 16);
				}
				if (reg == 0x7fff)
					tmp.back() += a1 & 7;
				else
					tmp.back() += 0xc0 | (reg << 3) | (a1 & 7);
				auto a2 = parse_imm(args[imm.first - 1]);
				short s2 = _sizes[p.first[imm.first][1] - 'A'];
				if (a2.second == -1) {
					cerr(linenum, error);
				} else if (a2.second == -2) {
					reloc.emplace_back(output_buffer.size() + tmp.size(), 0, ABS, args[imm.first - 1], std::max(s2, (short)32));
				} else if (a2.second == -3) {
					if (reloc_table.count(text_labels[a2.first])) {
						int32_t off = reloc_table.at(text_labels.at(a2.first)) - output_buffer.size() - 1 - _sizes[p.first[1][1] - 'A'] / 8;
						if ((int8_t)off == off) {
							a2.first = off;
						} else {
							reloc.emplace_back(output_buffer.size() + tmp.size(), 0, REL, text_labels[a2.first], 32);
							a2.first = 0;
						}
					} else {
						if (text_labels_instr[a2.first] - instr_cnt <= 9) {
							reloc.emplace_back(output_buffer.size() + tmp.size(), 0, REL, text_labels[a2.first], 8);
							a2.first = 0;
						} else {
							reloc.emplace_back(output_buffer.size() + tmp.size(), 0, REL, text_labels[a2.first], 32);
							a2.first = 0;
						}
					}
				} else if (a2.second == -5) {
					reloc.emplace_back(output_buffer.size() + tmp.size(), 0, REL, extern_labels[a2.first], 32);
					a2.first = 0;
				}
				for (int i = 0; i < s2; i += 8)
					tmp += (a2.first >> i) & 0xff;
			} else {
				short rex = 0;
				short rm = 0x7fff;
				short sib = 0x7fff;
				mem_output *data;
				if (mem.first != -1) {
					short s1 = mem.second;
					data = parse_mem(args[mem.first - 1], s1);
					if (data == nullptr) {
						if (error.empty())
							error = "mode d'adressage invalide";
						cerr(linenum, error);
					}
					if (data->prefix)
						tmp += data->prefix;
					rex = data->rex;
					rm = data->rm;
					sib = data->sib;
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
				if (rex != 0) {
					if (reg.first != -1)
						if (args[reg.first - 1][1] == 'h')
							cerr(linenum, "impossible d'utiliser un haut-demi registre avec une prefixe REX");
					tmp += rex;
				}
				for (size_t i = p.first.back()[0] == 'w'; i < p.first.back().size(); i += 2) {
					if (p.first.back()[i] == '/') {
						rm |= std::stoi(p.first.back().substr(i + 1, 1)) << 3;
						break;
					}
					tmp += std::stoi(p.first.back().substr(i, 2), nullptr, 16);
				}
				if (rm != 0x7fff)
					tmp += rm;
				if (sib != 0x7fff)
					tmp += sib;

				if (data->reloc.second != NONE) {
					reloc.emplace_back(output_buffer.size() + tmp.size(), data->offset, data->reloc.second, data->reloc.first, 32);
					data->offset = 0;
				}

				for (int i = 0; i < data->offsize; i += 8)
					tmp += (data->offset >> i) & 0xff;

				delete data;

				if (imm.first != -1) {
					auto a1 = parse_imm(args[imm.first - 1]);
					short s1 = _sizes[p.first[imm.first][1] - 'A'];
					if (a1.second == -1) {
						cerr(linenum, error);
					} else if (a1.second == -2) {
						reloc.emplace_back(output_buffer.size() + tmp.size(), 0, ABS, args[imm.first - 1], std::max(s1, (short)32));
					} else if (a1.second == -3) {
						if (reloc_table.count(text_labels[a1.first])) {
							int32_t off = reloc_table.at(text_labels.at(a1.first)) - output_buffer.size() - 1 - _sizes[p.first[1][1] - 'A'] / 8;
							if ((int8_t)off == off) {
								a1.first = off;
							} else {
								reloc.emplace_back(output_buffer.size() + tmp.size(), 0, REL, text_labels[a1.first], 32);
								a1.first = 0;
							}
						} else {
							if (text_labels_instr[a1.first] - instr_cnt <= 9) {
								reloc.emplace_back(output_buffer.size() + tmp.size(), 0, REL, text_labels[a1.first], 8);
								a1.first = 0;
							} else {
								reloc.emplace_back(output_buffer.size() + tmp.size(), 0, REL, text_labels[a1.first], 32);
								a1.first = 0;
							}
						}
					} else if (a1.second == -5) {
						reloc.emplace_back(output_buffer.size() + tmp.size(), 0, REL, extern_labels[a1.first], 32);
						a1.first = 0;
					}
					for (int i = 0; i < s1; i += 8)
						tmp += (a1.first >> i) & 0xff;
				}
			}
		}
		if (tmp.size() <= bestlen) {
			best = tmp;
			bestlen = tmp.size();
			bestreloc = reloc;
		}
		if (bestlen == 1)
			break;
	}
	for (auto reloc : bestreloc) {
		if (reloc.type != ABS)
			reloc.addend -= best.size() - (reloc.offset - output_buffer.size());
		relocations.push_back(reloc);
	}
	for (size_t i = 0; i < best.size(); i++)
		output_buffer.push_back(best[i]);
}
