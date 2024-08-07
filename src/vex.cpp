#include "vex.hpp"
#include "vex.dat"

void handle_vex(std::string &s, std::vector<std::string> &args, const size_t linenum, const bool prefix) {
	error = "";
	char *l = vex_map;
	char *r = vex_map + vex_map_size - 1;
	// go to the start of the section
	while (r[-1] != '\n' || r[-2] != '\n')
		r--;
	// binary search for the section that matches
	while (l < r && l < vex_map + vex_map_size - 2 && r > vex_map + 2) {
		char *m = l + (r - l) / 2;
		while (m >= vex_map + 2 && (m[-1] != '\n' || m[-2] != '\n'))
			m--;
		if (strncmp(m, s.c_str(), s.size()) == 0 && m[s.size()] == ' ') {
			l = m;
			break;
		} else if (strncmp(m, s.c_str(), s.size()) < 0) {
			l = m + 6;
			while (l <= vex_map + vex_map_size - 2 && (l[-1] != '\n' || l[-2] != '\n'))
				l++;
		} else {
			r = m - 6;
			while (r >= vex_map + 2 && (r[-1] != '\n' || r[-2] != '\n'))
				r--;
		}
	}
	// store all lines that match
	while ((strncmp(l, s.c_str(), s.size()) != 0 || l[s.size()] != ' ') && l < vex_map + vex_map_size)
		l++;
	if (l >= vex_map + vex_map_size - 1 || *(l - 1) != '\n')
		fatal(linenum, "instruction inconnue « " + s + " »");
	if (prefix)
		fatal(linenum, "impossible d'utiliser un préfixe avec une instruction VEX");
	r = std::find(l + 1, vex_map + vex_map_size, '\n');
	std::vector<std::string> matches;
	while (*l != '\n' && r != vex_map + vex_map_size) {
		matches.emplace_back(l, r);
		l = r + 1;
		r = std::find(l, vex_map + vex_map_size, '\n');
	}
	std::vector<std::pair<enum OperandType, short>> types;
	for (const std::string &arg : args) {
		enum OperandType type = get_optype(arg);
		if (type == REG) {
			types.emplace_back(REG, reg_size(arg));
		} else if (type == MEM) {
			types.emplace_back(MEM, mem_size(arg));
		} else if (type == IMM) {
			auto tmp = parse_imm(arg);
			if (tmp.second == -1) {
				fatal(linenum, error);
			} else if (tmp.second == -3) {
				if (reloc_table.count(text_labels[tmp.first])) {
					int32_t off = reloc_table.at(text_labels.at(tmp.first)) - text_buffer.size() - 3;
					if ((int8_t)off == off) {
						types.emplace_back(IMM, 8);
					} else {
						types.emplace_back(IMM, 32);
						tmp.first = 0;
					}
				} else {
					types.emplace_back(IMM, 32);
					tmp.first = 0;
				}
			} else if (tmp.second == -2 || tmp.second == -4) {
				types.emplace_back(IMM, 32);
			} else {
				types.emplace_back(IMM, tmp.second);
			}
		} else {
			fatal(linenum, "opérande invalide « " + arg + " »");
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
			} else {
				std::cerr << "Erreur : ensemble d'instructions mal configurée" << std::endl;
			}
		}
		if (matched) {
			for (size_t j = 0; j < args.size(); j++) {
				if (types[j].second == -1)
					types[j].second = _sizes[tokens[j + 1][1] - 'A'];
			}
			valid.emplace_back(tokens, size);
		}
	}
	if (valid.empty())
		fatal(linenum, "combination d'opcode et des opérandes invalide");
	for (size_t i = 1; i < valid.size(); i++) {
		if (valid[i].second != valid[i - 1].second)
			fatal(linenum, "taille d'opération non spécifiée");
	}
	std::string best = "";
	size_t bestlen = -1;
	std::vector<RelocEntry> bestreloc;
	for (auto &p : valid) {
		std::vector<RelocEntry> reloc;
		std::string tmp = "";
		// parse vex prefix
		std::string vex = p.first.back();
		short l = vex[0] - '0';
		short pp = vex[2] - '0';
		short mmmmm = vex[4] - '0';
		short w = vex[6] - '0';
		short rxb = 0;
		short vvvv = 0;
		if (args.size() == 0) {
			if ((rxb & 0b11) == 0 && mmmmm == 0 && w == 0) {
				tmp += (unsigned char)0xc5;
				tmp += (l << 2) | pp;
				tmp.back() ^= 0xf8;
			} else {
				tmp += (unsigned char)0xc4;
				tmp += mmmmm;
				tmp.back() ^= 0xe0;
				tmp += (w << 7) | (l << 2) | pp;
				tmp.back() ^= 0x78;
			}
			for (size_t i = 8; i < p.first.back().size(); i += 2)
				tmp += std::stoi(p.first.back().substr(i, 2), nullptr, 16);
		}
		if (args.size() == 1) {
			short s1 = _sizes[p.first[1][1] - 'A'];
			MemOutput *data = parse_mem(args[0], s1);
			if (data == nullptr) {
				if (error.empty())
					error = "mode d'adressage invalide";
				fatal(linenum, error);
			}
			if (data->prefix)
				tmp += data->prefix;
			if (data->rex)
				rxb = data->rex ^ 0x40;

			if ((rxb & 0b11) == 0 && mmmmm == 0 && w == 0) {
				tmp += (unsigned char)0xc5;
				tmp += (rxb << 5) | (l << 2) | pp;
				tmp.back() ^= 0xf8;
			} else {
				tmp += (unsigned char)0xc4;
				tmp += (rxb << 5) | mmmmm;
				tmp.back() ^= 0xe0;
				tmp += (w << 7) | (l << 2) | pp;
				tmp.back() ^= 0x78;
			}
			short reg = 0;
			for (size_t i = 8; i < p.first.back().size(); i += 2) {
				if (p.first.back()[i] == '/') {
					reg = std::stoi(p.first.back().substr(i + 1, 1), nullptr, 16);
					break;
				}
				tmp += std::stoi(p.first.back().substr(i, 2), nullptr, 16);
			}
			tmp += (uint8_t)data->rm | (reg << 3);
			if (data->sib != 0x7fff)
				tmp += data->sib;

			if (data->reloc.second != NONE) {
				reloc.emplace_back(text_buffer.size() + tmp.size(), data->offset, data->reloc.second, data->reloc.first, 32);
				data->offset = 0;
			}

			for (int i = 0; i < data->offsize; i += 8)
				tmp += (data->offset >> i) & 0xff;
		} else if (args.size() == 2) {
			short reg = 0x7fff;
			short mem_i = p.first[1][0] == 'M' ? 1 : 2;
			short reg_i = mem_i == 1 ? 2 : 1;
			short s1 = _sizes[p.first[mem_i][1] - 'A'];
			MemOutput *data = parse_mem(args[mem_i - 1], s1);
			if (data == nullptr) {
				if (error.empty())
					error = "mode d'adressage invalide";
				fatal(linenum, error);
			}
			if (data->prefix)
				tmp += data->prefix;
			if (data->rex)
				rxb = data->rex ^ 0x40;
			if (p.first.back()[p.first.back().size() - 2] == '/')
				reg = std::stoi(p.first.back().substr(p.first.back().size() - 1, 1), nullptr, 16);

			if (reg != 0x7fff)
				vvvv = reg_num(args[reg_i - 1]);
			else
				reg = reg_num(args[reg_i - 1]);

			if (reg >= 8) {
				rxb |= 0b100;
				reg ^= 8;
			}

			if ((rxb & 0b11) == 0 && mmmmm == 0 && w == 0) {
				tmp += (unsigned char)0xc5;
				tmp += (rxb << 5) | (vvvv << 3) | (l << 2) | pp;
				tmp.back() ^= 0xf8;
			} else {
				tmp += (unsigned char)0xc4;
				tmp += (rxb << 5) | mmmmm;
				tmp.back() ^= 0xe0;
				tmp += (w << 7) | (vvvv << 3) | (l << 2) | pp;
				tmp.back() ^= 0x78;
			}

			for (size_t i = 8; i < p.first.back().size(); i += 2) {
				if (p.first.back()[i] == '/')
					break;
				tmp += std::stoi(p.first.back().substr(i, 2), nullptr, 16);
			}
			tmp += (uint8_t)data->rm | (reg << 3);
			if (data->sib != 0x7fff)
				tmp += data->sib;

			if (data->reloc.second != NONE) {
				reloc.emplace_back(text_buffer.size() + tmp.size(), data->offset, data->reloc.second, data->reloc.first, 32);
				data->offset = 0;
			}

			for (int i = 0; i < data->offsize; i += 8)
				tmp += (data->offset >> i) & 0xff;
		} else if (args.size() == 3 || args.size() == 4) {
			if (p.first[3][0] == 'I' && (s.starts_with("vpsl") || s.starts_with("vpsr"))) {
				// special case: vvvv, r/m, imm
				vvvv = reg_num(args[0]);
				short rm = reg_num(args[1]);
				rxb = rm >= 8;

				if ((rxb & 0b11) == 0 && mmmmm == 0 && w == 0) {
					tmp += (unsigned char)0xc5;
					tmp += (rxb << 5) | (vvvv << 3) | (l << 2) | pp;
					tmp.back() ^= 0xf8;
				} else {
					tmp += (unsigned char)0xc4;
					tmp += (rxb << 5) | mmmmm;
					tmp.back() ^= 0xe0;
					tmp += (w << 7) | (vvvv << 3) | (l << 2) | pp;
					tmp.back() ^= 0x78;
				}

				short reg = 0;
				for (size_t i = 8; i < p.first.back().size(); i += 2) {
					if (p.first.back()[i] == '/') {
						reg = std::stoi(p.first.back().substr(i + 1, 1), nullptr, 16);
						break;
					}
					tmp += std::stoi(p.first.back().substr(i, 2), nullptr, 16);
				}
				tmp += 0xc0 | (reg << 3) | (rm & 7);
				tmp += std::stoi(args[2], nullptr, 0);
			} else {
				if (p.first[3][0] == 'I') {
					// reg, rm, imm
					short reg = reg_num(args[0]);
					short s1 = _sizes[p.first[2][1] - 'A'];
					MemOutput *data = parse_mem(args[1], s1);
					if (data == nullptr) {
						if (error.empty())
							error = "mode d'addressage invalide";
						fatal(linenum, error);
					}
					if (data->prefix)
						tmp += data->prefix;
					rxb = data->rex & 0x0f;
					if (reg >= 8) {
						reg &= 7;
						rxb |= 0b100;
					}
					if (!(rxb & 0b011) && !w && !mmmmm) {
						// short form
						tmp += (unsigned char)0xc5;
						tmp += (rxb << 5) | (l << 2) | pp;
						tmp.back() ^= 0xf8;
					} else {
						tmp += (unsigned char)0xc4;
						tmp += (rxb << 5) | mmmmm;
						tmp.back() ^= 0xe0;
						tmp += (w << 7) | (l << 2) | pp;
						tmp.back() ^= 0x78;
					}
					for (size_t i = 8; i < p.first.back().size(); i += 2)
						tmp += std::stoi(p.first.back().substr(i, 2), nullptr, 16);
					tmp += (reg << 3) | data->rm;
					if (data->sib != 0x7fff)
						tmp += data->sib;

					if (data->reloc.second != NONE) {
						reloc.emplace_back(text_buffer.size() + tmp.size(), data->offset, data->reloc.second, data->reloc.first, 32);
						data->offset = 0;
					}

					for (int i = 0; i < data->offsize; i += 8)
						tmp += (data->offset >> i) & 0xff;

					auto a3 = parse_imm(args[2]);
					for (int i = 0; i < _sizes[p.first[3][1] - 'A']; i += 8)
						tmp += (a3.first >> i) & 0xff;
				} else {
					short reg = 0x7fff;
					short mem_i = p.first[1][0] == 'M' ? 1 : 3;
					short reg_i = mem_i == 1 ? 3 : 1;
					short s1 = _sizes[p.first[mem_i][1] - 'A'];
					MemOutput *data = parse_mem(args[mem_i - 1], s1);
					if (data == nullptr) {
						if (error.empty())
							error = "mode d'adressage invalide";
						fatal(linenum, error);
					}
					if (data->prefix)
						tmp += data->prefix;
					if (data->rex)
						rxb = data->rex ^ 0x40;

					vvvv = reg_num(args[1]);
					reg = reg_num(args[reg_i - 1]);
					if (reg >= 8) {
						rxb |= 0b100;
						reg ^= 8;
					}

					if (!(rxb & 0b11) && !mmmmm && !w) {
						tmp += (unsigned char)0xc5;
						tmp += (rxb << 5) | (vvvv << 3) | (l << 2) | pp;
						tmp.back() ^= 0xf8;
					} else {
						tmp += (unsigned char)0xc4;
						tmp += (rxb << 5) | mmmmm;
						tmp.back() ^= 0xe0;
						tmp += (w << 7) | (vvvv << 3) | (l << 2) | pp;
						tmp.back() ^= 0x78;
					}

					for (size_t i = 8; i < p.first.back().size(); i += 2) {
						if (p.first.back()[i] == '/')
							break;
						tmp += std::stoi(p.first.back().substr(i, 2), nullptr, 16);
					}
					tmp += (reg << 3) | data->rm;
					if (data->sib != 0x7fff)
						tmp += data->sib;

					if (data->reloc.second != NONE) {
						reloc.emplace_back(text_buffer.size() + tmp.size(), data->offset, data->reloc.second, data->reloc.first, 32);
						data->offset = 0;
					}

					for (int i = 0; i < data->offsize; i += 8)
						tmp += (data->offset >> i) & 0xff;

					if (args.size() == 4) {
						std::pair<short, short> a4;
						if (types[3].first == IMM)
							a4 = {parse_imm(args[3]).first, _sizes[p.first[4][1] - 'A']};
						else
							a4 = {reg_num(args[3]) << 4, 8};
						for (int i = 0; i < a4.second; i += 8)
							tmp += (a4.first >> i) & 0xff;
					}
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
			reloc.addend -= best.size() - (reloc.offset - text_buffer.size());
		relocations.push_back(reloc);
	}
	for (size_t i = 0; i < best.size(); i++)
		text_buffer.push_back(best[i]);
}
