#include "main.hpp"

// input file
std::ifstream input;
// input file name
const char *input_name;
// output file
std::ofstream output;
// output file name
char *output_name;
// lines of input file
std::vector<std::string> lines;
// labels in data section (name, offset)
std::unordered_map<std::string, uint64_t> data_labels;
std::unordered_map<std::string, uint64_t> bss_labels;
std::vector<std::string> text_labels;
std::vector<std::string> extern_labels;
std::unordered_map<std::string, size_t> text_labels_map;
std::unordered_map<std::string, size_t> extern_labels_map;
// symbols (positions)
std::vector<reloc_entry> relocations;
std::unordered_map<std::string, std::pair<sect, size_t>> labels;
// symbol table (name, offset)
std::unordered_map<std::string, uint64_t> reloc_table;
// output buffer
std::vector<uint8_t> output_buffer;
uint64_t data_size = 0;
uint64_t bss_size = 0;
// last label that was not a dot
std::string prev_label;
// global symbols
std::unordered_set<std::string> global;

void print_help(const char *name) {
	std::cout << "Usage : " << name << " [options] fichier\n";
	std::cout << "Options :\n";
	std::cout << "-h, --help\t\tAfficher cette aide\n";
	std::cout << "-v, --version\t\tAfficher la version\n";
	std::cout << "-o, --output\t\tFichier de sortie\n";
}

void cerr(const int i, const std::string &msg) {
	std::cerr << input_name << ":" << i << ": erreur : " << msg << std::endl;
	if (output.is_open())
		output.close();
	unlink(output_name);
	exit(1);
}

int parse_args(int argc, char *argv[]) {
	if (argc < 2) {
		print_help(argv[0]);
		return 1;
	}
	for (int i = 1; i < argc; i++) {
		if (argv[i][0] != '-' || argv[i][1] == '\0') {
			input_name = argv[i];
			input.open(input_name);
			if (!input.is_open()) {
				std::cerr << "Erreur : Impossible d'ouvrir le fichier d'entrée " << input_name << std::endl;
				return 1;
			}
			continue;
		} else {
			if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
				print_help(argv[0]);
				exit(0);
			} else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
				printf("Version 0.0.1\n");
				exit(0);
			} else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
				if (i + 1 < argc) {
					output.open(argv[i + 1]);
					output_name = argv[i + 1];
					if (!output.is_open()) {
						std::cerr << "Erreur : Impossible d'ouvrir le fichier de sortie " << argv[i + 1] << std::endl;
						return 1;
					}
					i++;
				} else {
					std::cerr << "Erreur : Aucun fichier de sortie spécifié" << std::endl;
					return 1;
				}
			} else {
				fprintf(stderr, "Erreur : Option inconnue %s\n", argv[i]);
				return 1;
			}
		}
	}
	return 0;
}

const std::regex lead_trail("^\\s*(.*?)\\s*$"), between("\\s*([,+\\-*\\/:\\\"])\\s*");

void preprocess() {
	for (size_t i = 0; i < lines.size(); i++) {
		std::string &line = lines[i];
		// remove comments
		bool in_string = false;
		for (size_t i = 0; i < line.size(); i++) {
			if (line[i] == '"' && (i == 0 || line[i - 1] != '\\'))
				in_string = !in_string;
			if (line[i] == ';' && !in_string) {
				line = line.substr(0, i);
				break;
			}
		}
		if (in_string)
			cerr(i + 1, "chaîne de caractères non terminée");
		// remove leading and trailing whitespace
		line = std::regex_replace(line, lead_trail, "$1");
		// remove whitespace between tokens
		size_t l = 0;
		size_t r = line.find('"', l);
		while (r != 0 && r != std::string::npos) {
			if (line[r - 1] != '\\')
				break;
			r = line.find('"', r + 1);
		}
		if (r == std::string::npos) {
			line = std::regex_replace(line, between, "$1");
		} else {
			while (r != std::string::npos) {
				if (l == 0)
					line = std::regex_replace(line.substr(l, r - l), between, "$1") + line.substr(r);
				else
					line = line.substr(0, l) + std::regex_replace(line.substr(l, r - l - 1), between, "$1") + line.substr(r);
				l = r + 1;
				r = line.find('"', l);
				while (r != 0 && r != std::string::npos) {
					if (line[r - 1] != '\\')
						break;
					r = line.find('"', r + 1);
				}
				l = r + 1;
				r = line.find('"', l);
				while (r != 0 && r != std::string::npos) {
					if (line[r - 1] != '\\')
						break;
					r = line.find('"', r + 1);
				}
			}
			line = line.substr(0, l) + std::regex_replace(line.substr(l, line.size() - l), between, "$1");
		}
	}
}

void parse_labels() {
	sect curr_sect = UNDEF;
	for (size_t i = 0; i < lines.size(); i++) {
		std::string line = lines[i];
		while (line.size() == 0 && ++i < lines.size())
			line = lines[i];
		if (i == lines.size())
			break;
		if (line.starts_with("section ")) {
			if (line == "section .text") {
				curr_sect = TEXT;
			} else if (line == "section .data") {
				curr_sect = DATA;
			} else if (line == "section .bss") {
				curr_sect = BSS;
			} else {
				cerr(i + 1, "section inconnue « " + line.substr(8) + " »");
			}
			prev_label = "";
		} else if (line.find(':') != std::string::npos && line.find_first_of(" \t\"'") > line.find(':')) {
			if (line.size() == 1)
				cerr(i + 1, "étiquette vide");
			if (curr_sect == TEXT) {
				std::string label = line.substr(0, line.size() - 1);
				if (line[0] == '.') {
					if (prev_label == "")
						cerr(i + 1, "étiquette sans étiquette parente");
					label = prev_label + label;
				} else {
					prev_label = label;
				}
				line = label + ":";
				text_labels_map[label] = text_labels.size();
				text_labels.push_back(label);
			} else if (curr_sect == UNDEF) {
				cerr(i + 1, "étiquette hors d'une section");
			} else if (curr_sect == BSS) {
				if (line.starts_with("global ")) {
					std::string label = line.substr(7);
					if (label[0] == '.')
						cerr(i + 1, "étiquette locale dans une directive global");
					global.insert(label);
					continue;
				}
				std::string label = line.substr(0, line.find(':'));
				std::string instr = line.substr(line.find(':') + 1, line.find(' ') - line.find(':') - 1);
				size_t size = std::strtoull(line.substr(line.find(' ') + 1).c_str(), nullptr, 10);
				bss_labels[label] = bss_size;
				if (instr == "resb") {
					bss_size += size;
				} else if (instr == "resw") {
					bss_size += size * 2;
				} else if (instr == "resd") {
					bss_size += size * 4;
				} else if (instr == "resq") {
					bss_size += size * 8;
				} else {
					cerr(i + 1, "directive inconnue « " + instr + " »");
				}
			} else if (curr_sect == DATA) {
				if (line.starts_with("global ")) {
					std::string label = line.substr(7);
					if (label[0] == '.')
						cerr(i + 1, "étiquette locale dans une directive global");
					global.insert(label);
					continue;
				}
				std::string label = line.substr(0, line.find(':'));
				std::string instr = line.substr(line.find(':') + 1, line.find(' ') - line.find(':') - 1);
				std::vector<std::string> args;
				size_t pos = line.find(' ');
				while (pos != std::string::npos) {
					size_t next = line.find(',', pos + 1);
					while (next != std::string::npos) {
						std::string tmp = line.substr(0, next);
						if (std::count(tmp.begin(), tmp.end(), '\"') % 2 == 0)
							break;
						next = line.find(',', next + 1);
					}
					if (next == std::string::npos) {
						next = line.size();
						args.push_back(line.substr(pos + 1, next - pos - 1));
						break;
					}
					args.push_back(line.substr(pos + 1, next - pos - 1));
					pos = next;
				}
				size_t tmp = output_buffer.size();
				for (size_t i = 0; i < args.size(); i++) {
					if (args[i][0] == '"') {
						for (size_t j = 1; j < args[i].size() - 1; j++) {
							if (args[i][j] == '\\') {
								switch (args[i][j + 1]) {
								case '0':
									output_buffer.push_back('\0');
									break;
								case 'n':
									output_buffer.push_back('\n');
									break;
								case 'r':
									output_buffer.push_back('\r');
									break;
								case 't':
									output_buffer.push_back('\t');
									break;
								case '\\':
									output_buffer.push_back('\\');
									break;
								case '"':
									output_buffer.push_back('"');
									break;
								case 'x':
									output_buffer.push_back(std::stoul(args[i].substr(j + 2, 2), nullptr, 16));
									j += 2;
									break;
								}
								j++;
							} else
								output_buffer.push_back(args[i][j]);
						}
						continue;
					}
					if (instr == "db") {
						output_buffer.push_back(std::stoul(args[i], nullptr, 0));
					} else if (instr == "dw") {
						uint16_t val = std::stoul(args[i], nullptr, 0);
						output_buffer.push_back(val & 0xff);
						output_buffer.push_back((val >> 8) & 0xff);
					} else if (instr == "dd") {
						uint32_t val = std::stoul(args[i], nullptr, 0);
						output_buffer.push_back(val & 0xff);
						output_buffer.push_back((val >> 8) & 0xff);
						output_buffer.push_back((val >> 16) & 0xff);
						output_buffer.push_back((val >> 24) & 0xff);
					} else if (instr == "dq") {
						uint64_t val = std::stoull(args[i], nullptr, 0);
						output_buffer.push_back(val & 0xff);
						output_buffer.push_back((val >> 8) & 0xff);
						output_buffer.push_back((val >> 16) & 0xff);
						output_buffer.push_back((val >> 24) & 0xff);
						output_buffer.push_back((val >> 32) & 0xff);
						output_buffer.push_back((val >> 40) & 0xff);
						output_buffer.push_back((val >> 48) & 0xff);
						output_buffer.push_back((val >> 56) & 0xff);
					} else {
						cerr(i + 1, "directive inconnue « " + instr + " »");
					}
				}
				data_size += output_buffer.size() - tmp;
				data_labels[label] = tmp;
			}
		}
	}
}

void process_instructions() {
	sect curr_sect = UNDEF;
	for (size_t i = 0; i < lines.size(); i++) {
		std::string line = lines[i];
		while (line.size() == 0 && ++i < lines.size())
			line = lines[i];
		if (i == lines.size())
			break;
		if (line.starts_with("section ")) {
			if (line == "section .text") {
				curr_sect = TEXT;
			} else if (line == "section .data") {
				curr_sect = DATA;
			} else if (line == "section .bss") {
				curr_sect = BSS;
			}
		} else {
			if (curr_sect == TEXT) {
				// parse instruction
				std::string instr = line.substr(0, line.find(' '));
				if (instr.ends_with(':')) {
					instr = instr.substr(0, instr.size() - 1);
					if (instr[0] != '.')
						prev_label = instr.substr(0, instr.find('.'));
					else
						instr = prev_label + instr;
					reloc_table[instr] = output_buffer.size();
					continue;
				} else {
					for (size_t i = 0; i < instr.size(); i++)
						instr[i] = tolower(instr[i]);
				}
				if (instr == "global") {
					std::string label = line.substr(7);
					if (label[0] == '.') {
						std::cerr << "avertissement : " << input_name << ':' << i + 1 << ": étiquette locale dans une directive global" << std::endl;
						label = prev_label + label;
					}
					global.insert(label);
					continue;
				} else if (instr == "extern") {
					std::string label = line.substr(7);
					if (label[0] == '.')
						cerr(i + 1, "étiquette locale dans une directive extern");
					extern_labels.push_back(label);
					continue;
				}
				std::vector<std::string> args;
				size_t pos = line.find(' ');
				while (pos != std::string::npos) {
					size_t next = line.find(',', pos + 1);
					if (next == std::string::npos) {
						next = line.size();
						args.push_back(line.substr(pos + 1, next - pos - 1));
						break;
					}
					args.push_back(line.substr(pos + 1, next - pos - 1));
					pos = next;
				}
				handle(instr, args, i + 1);
			}
		}
	}
}

int main(int argc, char *argv[]) {
	if (parse_args(argc, argv))
		return 1;

	// if the file could not be opened it would have been caught earlier
	// so that means there was no attempt to open a file at all
	if (!input.is_open()) {
		std::cerr << "Erreur : aucun fichier d'entrée specifié" << std::endl;
		return 1;
	}
	if (!output.is_open()) {
		std::string tmp = std::string(input_name);
		tmp = tmp.substr(0, tmp.find_last_of('.')) + ".o";
		output_name = new char[tmp.size() + 1];
		memcpy(output_name, tmp.c_str(), tmp.size() + 1);
		output_name[tmp.size()] = '\0';
		output.open(output_name);
		if (!output.is_open()) {
			std::cerr << "Erreur : impossible d'ouvrir le fichier de sortie « a.out »" << std::endl;
			return 1;
		}
	}
	// load lines into vector
	std::string line;
	while (std::getline(input, line))
		lines.push_back(line);

	preprocess();

	parse_labels();

	// put all labels into a map
	for (auto l : data_labels) {
		labels[l.first] = {DATA, l.second};
	}
	for (auto l : bss_labels) {
		labels[l.first] = {BSS, l.second};
	}
	for (auto l : text_labels) {
		labels[l] = {TEXT, 0};
	}

	process_instructions();

	for (auto l : reloc_table) {
		labels[l.first] = {TEXT, l.second - data_size};
	}

	generate_elf(output, output_buffer, data_size, bss_size);

	return 0;
}
