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

std::ifstream input_file;
std::ofstream output_file;
// Rough position of the labels in the text section (for optimizing jumps)
std::vector<size_t> text_labels_instr;
std::vector<RelocEntry> relocations;
std::vector<Section> sections;
std::unordered_map<std::string, uint16_t> section_map;
std::unordered_map<std::string, Symbol> symbols;
// Last label that was not a dot
std::string prev_label;

[[noreturn]] void fatal(const int linenum, const std::string &msg) {
	std::cerr << input_name << ":" << linenum << ": error: " << msg << std::endl;
	if (output_file.is_open()) {
		output_file.close();
		remove(output_name);
	}
	exit(EXIT_FAILURE);
}

// https://llvm.org/docs/tutorial/MyFirstLanguageFrontend/LangImpl01.html
void lex(std::vector<Token> &tokens) {
	int linenum = 1;
	char c = input_file.get();
	// Uses tokens.back() as a buffer to accumulate characters until a token is complete
	tokens.push_back({NEWLINE, "", -1});
	while (!input_file.eof()) {
		while (isspace(c)) {
			if (c == '\n') {
				linenum++;
				if (tokens.size() >= 2 && tokens.rbegin()[1].type == NEWLINE)
					tokens.back().linenum++;
				else
					tokens.push_back({NEWLINE, "", linenum});
			}
			c = input_file.get();
		}

		if (c == ';') {
			// Comment
			while (c != '\n' && !input_file.eof())
				c = input_file.get();
		} else if (c == '"') {
			// String
			char last = 0;
			c = input_file.get();
			while (c != '"' && last != '\\' && c != '\n' && !input_file.eof()) {
				tokens.back().text += c;
				last = c;
				c = input_file.get();
			}
			if (c == '\n' || input_file.eof())
				fatal(linenum, "unterminated string");
			tokens.back().type = STRING;
			tokens.back().linenum = linenum;
			tokens.push_back({NEWLINE, "", linenum});
			c = input_file.get();
		} else if (c == '[') {
			// Memory reference (need this seperately because of the ':')
			while (c != ']' && c != '\n' && !input_file.eof()) {
				tokens.back().text += c;
				c = input_file.get();
			}
			if (c == '\n' || input_file.eof())
				fatal(linenum, "invalid memory reference");
			tokens.back().text += c;
			tokens.back().type = MEMORY;
			tokens.back().linenum = linenum;
			tokens.push_back({NEWLINE, "", linenum});
			c = input_file.get();
		} else {
			// Label/other
			while (!isspace(c) && c != ';' && c != ':' && c != ',' && !input_file.eof()) {
				tokens.back().text += c;
				c = input_file.get();
			}
			if (c == ',' && tokens.back().text.size() == 0) {
				c = input_file.get();
				continue;
			}
			if (c == ':') {
				// Label
				tokens.back().type = LABEL;
				tokens.back().linenum = linenum;
				tokens.push_back({NEWLINE, "", linenum});
				c = input_file.get();
			} else {
				// Other
				tokens.back().type = OTHER;
				tokens.back().linenum = linenum;
				tokens.push_back({NEWLINE, "", linenum});
			}
		}
	}
	tokens.pop_back();
	if (tokens.rbegin()[1].type == NEWLINE)
		tokens.pop_back();
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

void pad(int align, std::vector<uint8_t> &output_buffer) {
	int pad = output_buffer.size() % align;
	if (!pad)
		return;
	pad = align - pad;
	while (pad >= 11) {
		output_buffer.insert(output_buffer.end(), {0x66, 0x66, 0x66, 0x0f, 0x1f, 0x84, 0x90, 0x90, 0x90, 0x90, 0x90});
		pad -= 11;
	}
	if (pad == 1)
		output_buffer.insert(output_buffer.end(), {0x90});
	else if (pad == 2)
		output_buffer.insert(output_buffer.end(), {0x66, 0x90});
	else if (pad == 3)
		output_buffer.insert(output_buffer.end(), {0x0f, 0x1f, 0xc0});
	else if (pad == 4)
		output_buffer.insert(output_buffer.end(), {0x0f, 0x1f, 0x40, 0x90});
	else if (pad == 5)
		output_buffer.insert(output_buffer.end(), {0x0f, 0x1f, 0x44, 0x90, 0x90});
	else if (pad == 6)
		output_buffer.insert(output_buffer.end(), {0x66, 0x0f, 0x1f, 0x44, 0x90, 0x90});
	else if (pad == 7)
		output_buffer.insert(output_buffer.end(), {0x0f, 0x1f, 0x80, 0x90, 0x90, 0x90, 0x90});
	else if (pad == 8)
		output_buffer.insert(output_buffer.end(), {0x0f, 0x1f, 0x84, 0x90, 0x90, 0x90, 0x90, 0x90});
	else if (pad == 9)
		output_buffer.insert(output_buffer.end(), {0x66, 0x0f, 0x1f, 0x84, 0x90, 0x90, 0x90, 0x90, 0x90});
	else if (pad == 10)
		output_buffer.insert(output_buffer.end(), {0x66, 0x66, 0x0f, 0x1f, 0x84, 0x90, 0x90, 0x90, 0x90, 0x90});
}

void parse_labels(const std::vector<Token> &tokens) {
	std::string curr_sect = "";

	for (int i = 0; i < tokens.size(); i++) {
		const Token &curr_token = tokens[i];
		if (curr_token.type == OTHER) {
			if (curr_token.text == "section") {
				curr_sect = tokens[++i].text;
				if (!section_map.count(curr_sect)) {
					section_map[curr_sect] = sections.size();
					sections.push_back({curr_sect, {}});
				}
			} else if (curr_token.text == "global") {
				const std::string &label = tokens[++i].text;
				if (label[0] == '.')
					fatal(curr_token.linenum, "local label in global directive");
				if (!symbols.count(label))
					symbols[label] = (Symbol){label, true, SYMTYPE_NONE, 0, -2};
				else
					symbols[label].global = true;
			} else if (curr_token.text == "extern") {
				const std::string &label = tokens[++i].text;
				if (label[0] == '.')
					fatal(curr_token.linenum, "local label in extern directive");
				if (symbols.count(label) && symbols[label].value != -2)
					fatal(curr_token.linenum, "duplicate definition of label `" + label + "'");
				symbols[label] = (Symbol){label, false, SYMTYPE_NONE, section_map[curr_sect] + 1, 0};
			}
		} else if (curr_token.type == LABEL) {
			if (curr_token.text[0] == '.') {
				if (prev_label.empty())
					fatal(curr_token.linenum, "local label in global scope");
				symbols[prev_label + curr_token.text] = (Symbol){prev_label + curr_token.text, false, SYMTYPE_NONE, section_map[curr_sect] + 1, -1};
			} else {
				if (symbols.count(curr_token.text)) {
					if (symbols[curr_token.text].value != -2)
						fatal(curr_token.linenum, "duplicate definition of label `" + curr_token.text + "'");
					symbols[curr_token.text].shndx = section_map[curr_sect] + 1;
				} else {
					symbols[curr_token.text] = (Symbol){curr_token.text, false, SYMTYPE_NONE, section_map[curr_sect] + 1, -1};
				}
				prev_label = curr_token.text;
			}
		}
	}
}

void process_instructions(const std::vector<Token> &tokens) {
	std::string curr_sect = "";
	prev_label = "";

	for (int i = 0; i < tokens.size(); i++) {
		const Token &curr_token = tokens[i];
		if (curr_token.type == OTHER) {
			const std::string &instr = curr_token.text;
			int linenum = curr_token.linenum;
			if (instr == "section") {
				curr_sect = tokens[++i].text;
			} else if (instr == "global" || instr == "extern") {
				i++;
			} else if (instr == "align") {
				try {
					pad(std::stoi(tokens[++i].text), sections[section_map[curr_sect]].data);
				} catch (std::invalid_argument &e) {
					fatal(linenum, "invalid argument to align directive");
				}
			} else if (instr.starts_with("res") && instr.size() == 4) {
				try {
					int size = std::stoi(instr.substr(3));
					if (size < 0)
						fatal(linenum, "invalid argument to res directive");
					sections[section_map[curr_sect]].data.resize(sections[section_map[curr_sect]].data.size() + size);
				} catch (std::invalid_argument &e) {
					fatal(linenum, "invalid argument to res directive");
				}
			} else if (instr[0] == 'd' && instr.size() == 2) {
				// parse_d(instr, args, linenum, sections[section_map[curr_sect]].data);
			} else {
				std::vector<std::string> args;
				while (tokens[++i].type != NEWLINE)
					args.push_back(tokens[i].text);

				// handle(curr_token.text, args, linenum, instr_cnt);
			}
		} else if (curr_token.type == LABEL) {
			if (curr_token.text[0] == '.') {
				symbols[prev_label + curr_token.text].value = sections[section_map[curr_sect]].data.size();
			} else {
				symbols[curr_token.text].value = sections[section_map[curr_sect]].data.size();
				prev_label = curr_token.text;
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
		output_file.open(output_name, std::ios::binary);
		delete[] output_name;
	} else {
		output_file.open(output_name, std::ios::binary);
	}
	if (!output_file.is_open()) {
		std::cerr << "Error: Couldn't open output file \"" << output_name << '"' << std::endl;
		return EXIT_FAILURE;
	}

	// Lex the input file
	std::vector<Token> tokens;
	lex(tokens);

	parse_labels(tokens);

	process_instructions(tokens);

	std::vector<Symbol> symbols_vec;
	for (auto &sym : symbols)
		symbols_vec.emplace_back(sym.second);

	if (output_format == ELF)
		ELF_write(sections, symbols_vec, output_file);
	// else if (output_format == COFF)
	// 	generate_coff(output_file, bss_size);

	return EXIT_SUCCESS;
}
