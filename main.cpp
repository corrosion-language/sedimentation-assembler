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
std::vector<std::string> text_labels;
std::unordered_map<std::string, int> text_labels_map;
// symbols (positions)
std::vector<uint64_t> relocations;
// symbol table (name, offset)
std::unordered_map<std::string, uint64_t> reloc_table;
// output buffer
std::vector<uint8_t> output_buffer;
uint64_t data_offset = 0;
uint64_t bss_offset = 0;
uint64_t text_offset = 0;
uint64_t data_size = 0;

std::string &filename(void) { return input_name; }

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
	for (size_t i = 0; i < lines.size(); i++) {
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
		// remove whitespace before and after commas
		// check if we are in a string
		bool in_string = false;
		for (size_t j = 0; j < line.size(); j++) {
			if (line[j] == '"' && (j == 0 || line[j - 1] != '\\'))
				in_string = !in_string;
			if (line[j] == ',' && !in_string) {
				size_t k = line.find_first_not_of(" \t", j + 1);
				if (k != std::string::npos) {
					k--;
					while (k != j)
						line.erase(k--, 1);
				}
				k = line.find_last_not_of(" \t", j - 1);
				if (k != std::string::npos) {
					k++;
					while (k != j)
						line.erase(k++, 1);
				}
			}
		}
	}

	// go line by line and parse labels
	sect curr_sect = UNDEF;
	// last label that was not a dot
	std::string prev_label;
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
			data_size = 0;
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
			} else if (curr_sect == UNDEF) {
				std::cerr << input_name << ':' << i + 1 << ": error: label outside of section" << std::endl;
				return 1;
			}
		}
	}

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
				if (instr.ends_with(':'))
					continue;
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
			}
		}
	}
	return 0;
}
