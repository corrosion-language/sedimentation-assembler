#include "main.hpp"

// input file
std::ifstream input;
// input file name
std::string input_name;
// output file
std::ofstream output;
// lines of input file
std::vector<std::string> lines;
// labels in data section (name, offset)
std::unordered_map<std::string, uint64_t> data_labels;
// labels in code section (idx, name)
std::vector<std::string> text_labels;
// symbols (positions)
std::vector<uint64_t> relocations;
// symbol table (idx, offset)
std::vector<uint64_t> reloc_table;

void print_help(const char *name) {
	std::cout << "Usage: " << name << " [options] input\n";
	std::cout << "Options:\n";
	std::cout << "-h, --help\t\tPrint this help message\n";
	std::cout << "-v, --version\t\tPrint version information\n";
	std::cout << "-o, --output\t\tSpecify output file" << std::endl;
}

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

int op_type(std::string s) {
	if (reg_size(s) != -1)
		return REG;
	if (s.find(' ') != std::string::npos || s[0] == '[')
		return MEM;
	// if in symbol table return IMM
	return IMM;
	return -1;
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
	for (int i = 0; i < lines.size(); i++) {
		std::string &line = lines[i];
		// remove comments
		line = line.substr(0, line.find(';'));
		// remove leading whitespace
		size_t first = line.find_first_not_of(" \t");
		if (first == std::string::npos)
			line = "";
		else
			line = line.substr(first);
		// remove trailing whitespace
		size_t last = line.find_last_not_of(" \t");
		if (last == std::string::npos)
			line = "";
		else
			line = line.substr(0, last + 1);
	}
	// go line by line and parse labels
	sect curr_sect = UNDEF;
	uint64_t data_size = 0;
	// last label that was not a dot
	std::string last_tl = "";
	for (int i = 0; i < lines.size(); i++) {
		std::string &line = lines[i];
		while (line.size() == 0 && i + 1 < lines.size()) {
			i++;
			if (i >= lines.size())
				break;
			line = lines[i];
		}
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
			data_size = 0;
			last_tl = "";
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
					int l = line.find('\"', line.find(':'));
					int r = line.find('\"', l + 1);
					while (l != std::string::npos && r != std::string::npos) {
						len += r - l - 3;
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
					if (last_tl == "") {
						std::cerr << input_name << ':' << i + 1 << ": error: dot label without text label" << std::endl;
						return 1;
					}
					label = last_tl + label;
				} else {
					last_tl = label;
				}
				text_labels.push_back(label);
			} else if (curr_sect == UNDEF) {
				std::cerr << input_name << ':' << i + 1 << ": error: label outside of section" << std::endl;
				return 1;
			}
		}
	}
	return 0;
}
