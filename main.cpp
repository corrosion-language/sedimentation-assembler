#include "main.hpp"

// input file
std::ifstream input;
// input file name
const char *input_name;
// output file
std::ofstream output;
// output file name
const char *output_name;
// lines of input file
std::vector<std::string> lines;
// labels in data section (name, offset)
std::unordered_map<std::string, uint64_t> data_labels;
std::vector<std::string> text_labels;
std::unordered_map<std::string, size_t> text_labels_map;
// symbols (positions)
std::vector<std::pair<uint32_t, short>> text_relocations;
std::vector<size_t> data_relocations;
// symbol table (name, offset)
std::unordered_map<std::string, uint64_t> reloc_table;
// output buffer
std::vector<uint8_t> output_buffer;
uint64_t data_size = 0;
uint64_t bss_size = 0;
uint64_t cum = 0;
uint64_t num = 0;
// last label that was not a dot
std::string prev_label;

void print_help(const char *name) {
	std::cout << "Usage: " << name << " [options] input\n";
	std::cout << "Options:\n";
	std::cout << "-h, --help\t\tPrint this help message\n";
	std::cout << "-v, --version\t\tPrint version information\n";
	std::cout << "-o, --output\t\tSpecify output file" << std::endl;
}

int main(int argc, char *argv[]) {
	if (argc < 2) {
		print_help(argv[0]);
		return 1;
	}
	for (int i = 1; i < argc; i++) {
		if (argv[i][0] != '-' || argv[i][1] == '\0') {
			input_name = argv[i];
			input.open(input_name);
			if (!input.is_open()) {
				std::cerr << "Error: Could not open input file " << input_name << std::endl;
				return 1;
			}
			continue;
		} else {
			if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
				print_help(argv[0]);
				return 0;
			} else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
				printf("Version 0.0.1\n");
				return 0;
			} else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
				if (i + 1 < argc) {
					output.open(argv[i + 1]);
					output_name = argv[i + 1];
					if (!output.is_open()) {
						std::cerr << "Error: Could not open output file " << argv[i + 1] << std::endl;
						return 1;
					}
					i++;
				} else {
					std::cerr << "Error: No output file specified" << std::endl;
					return 1;
				}
			} else {
				fprintf(stderr, "Error: Unknown option %s\n", argv[i]);
				return 1;
			}
		}
	}
	if (!input.is_open()) {
		std::cerr << "Error: No input file specified" << std::endl;
		return 1;
	}
	if (!output.is_open()) {
		output.open("a.out");
		output_name = "a.out";
		if (!output.is_open()) {
			std::cerr << "Error: Could not open output file a.out" << std::endl;
			return 1;
		}
	}
	// load lines into vector
	std::string line;
	while (std::getline(input, line)) {
		lines.push_back(line);
	}

	// perform preprocessing
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
			std::cerr << input_name << ":" << i + 1 << ": error: unterminated string constant" << std::endl;
		// remove leading and trailing whitespace
		profile(line = std::regex_replace(line, std::regex("^\\s*(.*?)\\s*$"), "$1"));
		// remove whitespace between tokens
		size_t l = 0;
		size_t r = line.find('"', l);
		while (r != 0 && r != std::string::npos) {
			if (line[r - 1] != '\\')
				break;
			r = line.find('"', r + 1);
		}
		std::regex p("\\s*([,+\\-*\\/:\\\"])\\s*");
		if (r == std::string::npos) {
			profile(line = std::regex_replace(line, p, "$1"))
		} else {
			while (r != std::string::npos) {
				if (l == 0)
					line = std::regex_replace(line.substr(l, r - l), p, "$1") + line.substr(r);
				else
					line = line.substr(0, l) + std::regex_replace(line.substr(l, r - l - 1), p, "$1") + line.substr(r);
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
			line = line.substr(0, l) + std::regex_replace(line.substr(l, line.size() - l), p, "$1");
		}
	}
	std::cout << "done preprocessing" << std::endl;
	// go line by line and parse labels
	sect curr_sect = UNDEF;
	for (size_t i = 0; i < lines.size(); i++) {
		std::string &line = lines[i];
		while (line.size() == 0 && ++i < lines.size())
			line = lines[i];
		if (i == lines.size())
			break;
		if (line.starts_with("section ")) {
			if (line == "section .text") {
				curr_sect = TEXT;
			} else if (line == "section .rodata") {
				curr_sect = RODATA;
			} else if (line == "section .bss") {
				curr_sect = BSS;
			} else {
				std::cerr << input_name << ':' << i + 1 << ": error: unknown section " << line.substr(8) << std::endl;
				return 1;
			}
			prev_label = "";
		} else if (line.find(':') != std::string::npos && line.find_first_of(" \t\"'") > line.find(':')) {
			if (line.size() == 1) {
				std::cerr << input_name << ':' << i + 1 << ": error: empty label" << std::endl;
				return 1;
			}
			if (curr_sect == RODATA) {
				data_labels[line.substr(0, line.find(':'))] = data_size;
				uint64_t len = std::count(line.begin(), line.end(), ',') + 1;
				char c = line[line.find('d', line.find(':')) + 1];
				if (c == 'b') {
					size_t l = line.find('\"', line.find(':'));
					while (l != 0 && l != std::string::npos && line[l - 1] == '\\')
						l = line.find('\"', l + 1);
					size_t r = line.find('\"', l + 1);
					while (r != 0 && r != std::string::npos && line[r - 1] == '\\')
						r = line.find('\"', r + 1);
					while (l != std::string::npos && r != std::string::npos) {
						len += r - l - 2;
						l = line.find('\"', r + 1);
						r = line.find('\"', l + 1);
					}
					if (l != std::string::npos) {
						std::cerr << input_name << ':' << i + 1 << ": error: unterminated string constant" << std::endl;
						return 1;
					}
				} else if (c == 'w')
					len *= 2;
				else if (c == 'd')
					len *= 4;
				else if (c == 'q')
					len *= 8;
				data_size += len;

			} else if (curr_sect == TEXT) {
				std::string label = line.substr(0, line.size() - 1);
				if (line[0] == '.') {
					if (prev_label == "") {
						std::cerr << input_name << ':' << i + 1 << ": error: dot label without parent label" << std::endl;
						return 1;
					}
					label = prev_label + label;
				} else {
					prev_label = label;
				}
				line = label + ":";
				text_labels_map[label] = text_labels.size();
				text_labels.push_back(label);
			} else if (curr_sect == BSS) {
				std::string label = line.substr(0, line.find(':'));
				data_labels[label] = bss_size;
				char c = line[line.find("res") + 3];
				int len = std::stoi(line.substr(line.find("res") + 5));
				if (c == 'w')
					len *= 2;
				else if (c == 'd')
					len *= 4;
				else if (c == 'q')
					len *= 8;
				bss_size += len;
			} else if (curr_sect == UNDEF) {
				std::cerr << input_name << ':' << i + 1 << ": error: label outside of section" << std::endl;
				return 1;
			}
		}
	}

	std::cout << "Average time: " << ((double)cum / (double)num * 2 / 1000) << "us" << std::endl;

	curr_sect = UNDEF;
	// go line by line and parse instructions
	for (size_t i = 0; i < lines.size(); i++) {
		std::string &line = lines[i];
		while (line.size() == 0 && ++i < lines.size())
			line = lines[i];
		if (i == lines.size())
			break;
		if (line.starts_with("section ")) {
			if (line == "section .text") {
				curr_sect = TEXT;
			} else if (line == "section .rodata") {
				curr_sect = RODATA;
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
					reloc_table[instr] = output_buffer.size() + bss_size;
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
				if (!handle(instr, args, i + 1))
					return 1;
			} else if (curr_sect == RODATA) {
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
				reloc_table[label] = output_buffer.size() + bss_size;
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
						std::cerr << input_name << ':' << i + 1 << ": error: unknown directive " << args[1] << std::endl;
						return 1;
					}
				}
			} else if (curr_sect == BSS) {
				std::string label = line.substr(0, line.find(':'));
				std::string instr = line.substr(line.find(':') + 1, line.find(' ') - line.find(':') - 1);
				size_t size = std::strtoull(line.substr(line.find(' ') + 1).c_str(), nullptr, 10);
				data_labels[label] = output_buffer.size() + bss_size;
				if (instr == "resb") {
					bss_size += size;
				} else if (instr == "resw") {
					bss_size += size * 2;
				} else if (instr == "resd") {
					bss_size += size * 4;
				} else if (instr == "resq") {
					bss_size += size * 8;
				} else {
					std::cerr << input_name << ':' << i + 1 << ": error: unknown directive " << instr << std::endl;
					return 1;
				}
			}
		}
	}

	// text relocations
	for (std::pair<uint64_t, short> reloc : text_relocations) {
		uint32_t symid = *(uint32_t *)(output_buffer.data() + reloc.first);
		int32_t pos = reloc_table.at(text_labels[symid]) - reloc.first - 4;
		*(uint32_t *)(output_buffer.data() + reloc.first) = pos;
	}

	generate_elf(output, output_buffer, data_size, bss_size);

	// make file executable
	chmod(output_name, S_IRWXU | S_IRWXG | S_IXOTH | S_IROTH);

	return 0;
}
