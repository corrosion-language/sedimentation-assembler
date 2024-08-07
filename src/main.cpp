#include "main.hpp"

// ARGUMENT PARSING
const char *input_name;
char *output_name;
#ifdef WINDOWS
OutputFormat output_format = COFF;
#elif defined(MACOS)
OutputFormat output_format = MACHO;
#else
OutputFormat output_format = ELF;
#endif

// https://www.gnu.org/software/libc/manual/html_node/Argp-Parser-Functions.html
error_t parse_opt(int key, char *arg, struct argp_state *state) {
	switch (key) {
	case 'o':
		if (output_name != nullptr)
			argp_usage(state);
		output_name = arg;
		break;
	case 'f':
		if (strcmp(arg, "elf") == 0 || strcmp(arg, "elf64") == 0) {
			output_format = ELF;
		} else if (strcmp(arg, "coff") == 0) {
			output_format = COFF;
		} else if (strcmp(arg, "macho") == 0) {
			output_format = MACHO;
		} else {
			argp_usage(state);
		}
		break;
	case ARGP_KEY_ARG:
		if (input_name != nullptr)
			argp_usage(state);
		input_name = arg;
		break;
	case ARGP_KEY_END:
		if (input_name == nullptr)
			argp_usage(state);
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

const char *argp_program_version = "sedimentation 1.0.1";
const char *argp_program_bug_address = "<wdotmathree+sedimentation@gmail.com>";
static char doc[] = "Assembler for x86-64 (PZAssembler)";

// https://www.gnu.org/software/libc/manual/html_node/Argp-Option-Vectors.html
static struct argp_option options[] = {
	{"output", 'o', "FILE", 0, "Output file", 0},
	{"format", 'f', "FORMAT", 0, "Output format (elf, coff, macho)", 0},
	{},
};
// https://www.gnu.org/software/libc/manual/html_node/Argp-Parsers.html
static struct argp argp = {
	.options = options,
	.parser = parse_opt,
	.args_doc = "filename",
	.doc = doc,
	.children = nullptr,
	.help_filter = nullptr,
	.argp_domain = nullptr,
};

// input file
std::ifstream input_file;
// output file
std::ofstream output;
// lines of input file
std::vector<std::string> lines;
// labels in data section (name, offset)
std::unordered_map<std::string, uint64_t> data_labels;
std::unordered_map<std::string, uint64_t> rodata_labels;
std::unordered_map<std::string, uint64_t> bss_labels;
std::vector<std::string> text_labels;
std::vector<std::string> extern_labels;
std::unordered_map<std::string, size_t> text_labels_map;
std::unordered_map<std::string, size_t> extern_labels_map;
std::vector<size_t> text_labels_instr;
// symbols (positions)
std::vector<RelocEntry> relocations;
std::unordered_map<std::string, std::pair<Section, size_t>> labels;
// symbol table (name, offset)
std::unordered_map<std::string, uint64_t> reloc_table;
// output buffer
std::string text_buffer;
std::string data_buffer;
std::string rodata_buffer;
uint64_t bss_size = 0;
// last label that was not a dot
std::string prev_label;
// global symbols
std::unordered_set<std::string> global;

void fatal(const int linenum, const std::string &msg) {
	std::cerr << input_name << ":" << linenum << ": error: " << msg << std::endl;
	if (output.is_open()) {
		output.close();
		remove(output_name);
	}
	exit(EXIT_FAILURE);
}

// Regex patterns for preprocessing (removing whitespace)
const std::regex lead_trail(R"(^\s*(.*?)\s*$)"), between(R"(\s*([,+\-*\/:\\"])\s*)");

void preprocess() {
	for (size_t i = 0; i < lines.size(); i++) {
		std::string &line = lines[i];
		// Remove comments
		bool in_string = false;
		for (size_t j = 0; j < line.size(); j++) {
			if (line[j] == '"' && (j == 0 || line[j - 1] != '\\'))
				in_string = !in_string;
			if (line[j] == ';' && !in_string) {
				line = line.substr(0, j);
				break;
			}
		}
		if (in_string)
			cerr(i + 1, "unterminated string");
		// Remove leading and trailing whitespace
		line = std::regex_replace(line, lead_trail, "$1");
		// Remove whitespace between tokens
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
				l = line.find('"', r + 1);
				while (l != 0 && l != std::string::npos) {
					if (line[l - 1] != '\\')
						break;
					l = line.find('"', l + 1);
				}
				l++;
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

void parse_d(std::string &instr, std::vector<std::string> &args, size_t line, std::string &output_buffer) {
	for (size_t i = 0; i < args.size(); i++) {
		if (args[i][0] == '"') {
			for (size_t j = 1; j < args[i].size() - 1; j++) {
				if (args[i][j] == '\\') {
					switch (args[i][j + 1]) {
					case '0':
						output_buffer += '\0';
						break;
					case 'n':
						output_buffer += '\n';
						break;
					case 'r':
						output_buffer += '\r';
						break;
					case 't':
						output_buffer += '\t';
						break;
					case '\\':
						output_buffer += '\\';
						break;
					case '"':
						output_buffer += '"';
						break;
					case '\'':
						output_buffer += '\'';
						break;
					case 'x':
						output_buffer += std::stoul(args[i].substr(j + 2, 2), nullptr, 16);
						j += 2;
						break;
					}
					j++;
				} else
					output_buffer += args[i][j];
			}
			continue;
		}
		if (args[i][0] == '\'') {
			output_buffer.push_back(args[i][1]);
		} else if (instr == "db") {
			output_buffer += std::stoul(args[i], nullptr, 0);
		} else if (instr == "dw") {
			uint16_t val = std::stoul(args[i], nullptr, 0);
			output_buffer += val & 0xff;
			output_buffer += (val >> 8) & 0xff;
		} else if (instr == "dd") {
			uint32_t val = std::stoul(args[i], nullptr, 0);
			output_buffer += val & 0xff;
			output_buffer += (val >> 8) & 0xff;
			output_buffer += (val >> 16) & 0xff;
			output_buffer += (val >> 24) & 0xff;
		} else if (instr == "dq") {
			uint64_t val = std::stoull(args[i], nullptr, 0);
			output_buffer += val & 0xff;
			output_buffer += (val >> 8) & 0xff;
			output_buffer += (val >> 16) & 0xff;
			output_buffer += (val >> 24) & 0xff;
			output_buffer += (val >> 32) & 0xff;
			output_buffer += (val >> 40) & 0xff;
			output_buffer += (val >> 48) & 0xff;
			output_buffer += (val >> 56) & 0xff;
		} else {
			fatal(line + 1, "directive inconnue « " + instr + " »");
		}
	}
}

void pad(int align, std::string &output_buffer) {
	int pad = output_buffer.size() % align;
	if (!pad)
		return;
	pad = align - pad;
	while (pad >= 11) {
		output_buffer += "\x66\x66\x66\x0f\x1f\x84\x90\x90\x90\x90\x90";
		pad -= 11;
	}
	if (pad == 1)
		output_buffer += '\x90';
	else if (pad == 2)
		output_buffer += "\x66\x90";
	else if (pad == 3)
		output_buffer += "\x0f\x1f\xc0";
	else if (pad == 4)
		output_buffer += "\x0f\x1f\x40\x90";
	else if (pad == 5)
		output_buffer += "\x0f\x1f\x44\x90\x90";
	else if (pad == 6)
		output_buffer += "\x66\x0f\x1f\x44\x90\x90";
	else if (pad == 7)
		output_buffer += "\x0f\x1f\x80\x90\x90\x90\x90";
	else if (pad == 8)
		output_buffer += "\x0f\x1f\x84\x90\x90\x90\x90\x90";
	else if (pad == 9)
		output_buffer += "\x66\x0f\x1f\x84\x90\x90\x90\x90\x90";
	else if (pad == 10)
		output_buffer += "\x66\x66\x0f\x1f\x84\x90\x90\x90\x90\x90";
}

void parse_labels() {
	sect curr_sect = UNDEF;
	size_t instr_cnt = 0;
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
			} else if (line == "section .rodata") {
				curr_sect = RODATA;
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
				text_labels_instr.push_back(instr_cnt);
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
			} else if (curr_sect == DATA || curr_sect == RODATA) {
				std::string &output_buffer = curr_sect == DATA ? data_buffer : rodata_buffer;
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
				parse_d(instr, args, i, output_buffer);
				(curr_sect == DATA ? data_labels : rodata_labels)[label] = tmp;
			}
		} else if (curr_sect == TEXT && !line.starts_with("global ") && !line.starts_with("extern ") && !line.starts_with("align")) {
			if (line[0] != 'd' || line[2] != ' ') {
				instr_cnt++;
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
			size_t delta = 0;
			if (line[1] == 'b') {
				for (const auto &arg : args) {
					if (arg[0] == '"')
						delta += arg.size() - 2;
					else
						delta++;
				}
			} else if (line[1] == 'w') {
				delta += args.size() * 2;
			} else if (line[1] == 'd') {
				delta += args.size() * 4;
			} else {
				delta += args.size() * 8;
			}
			instr_cnt += delta / 15 + !!(delta % 15);
		} else if (line.starts_with(".align ")) {
			int align = std::stoi(line.substr(7));
			if (curr_sect == TEXT) {
				instr_cnt += (align + 14) / 15;
				continue;
			}
			pad(align, curr_sect == DATA ? data_buffer : rodata_buffer);
		}
	}
}

void process_instructions() {
	sect curr_sect = UNDEF;
	size_t instr_cnt = 0;
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
					if (instr[0] != '.') {
						prev_label = instr.substr(0, instr.find('.'));
						pad(16, text_buffer);
					} else {
						instr = prev_label + instr;
					}
					reloc_table[instr] = text_buffer.size();
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
					extern_labels_map[label] = extern_labels.size();
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
				if (instr[0] == 'd' && instr.size() == 2) {
					parse_d(instr, args, i, text_buffer);
				} else if (instr == "align") {
					pad(std::stoi(args[0]), text_buffer);
				} else {
					handle(instr, args, i + 1, instr_cnt);
				}
				instr_cnt++;
			}
		}
	}
}

int main(int argc, char *argv[]) {
	if (argp_parse(&argp, argc, argv, 0, NULL, NULL))
		return EXIT_FAILURE;

	if (input_name == nullptr) {
		std::cerr << "Error: No input file specified" << std::endl;
		return EXIT_FAILURE;
	}
	input_file.open(input_name, std::ios::in);
	if (!input_file.is_open()) {
		std::cerr << "Error: Couldn't open input file" << std::endl;
		return EXIT_FAILURE;
	}
	if (output_name == nullptr) {
		std::string tmp = std::string(input_name);
		tmp = tmp.substr(0, tmp.find_last_of('.'));
		if (output_format == ELF || output_format == MACHO)
			tmp += ".o";
		else if (output_format == COFF)
			tmp += ".obj";
		output_name = new char[tmp.size() + 1];
		memcpy(output_name, tmp.c_str(), tmp.size() + 1);
		output_name[tmp.size()] = '\0';
		output.open(output_name, std::ios::binary);
		delete[] output_name;
	} else {
		output.open(output_name, std::ios::binary);
	}
	if (!output.is_open()) {
		std::cerr << "Error: Couldn't open output file \"" << output_name << '"' << std::endl;
		return EXIT_FAILURE;
	}

	// Load lines into vector
	std::string line;
	while (std::getline(input, line))
		lines.push_back(line);

	preprocess();

	parse_labels();

	// put all labels into a map
	for (const auto &l : data_labels) {
		labels[l.first] = {DATA, l.second};
	}
	for (const auto &l : rodata_labels) {
		labels[l.first] = {RODATA, l.second};
	}
	for (const auto &l : bss_labels) {
		labels[l.first] = {BSS, l.second};
	}
	for (const auto &l : text_labels) {
		labels[l] = {TEXT, 0};
	}

	process_instructions();

	for (const auto &l : reloc_table)
		labels[l.first] = {TEXT, l.second};

	if (output_format == ELF)
		generate_elf(output, bss_size);
	else if (output_format == COFF)
		generate_coff(output, bss_size);

	return EXIT_SUCCESS;
}
