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
std::ofstream output;
// Rough position of the labels in the text section (for optimizing jumps)
std::vector<size_t> text_labels_instr;
std::vector<RelocEntry> relocations;
std::vector<Section> sections;
std::unordered_map<std::string, size_t> section_map;
std::unordered_map<std::string, Symbol> symbols;
// Last label that was not a dot
std::string prev_label;

void fatal(const int linenum, const std::string &msg) {
	std::cerr << input_name << ":" << linenum << ": error: " << msg << std::endl;
	if (output.is_open()) {
		output.close();
		remove(output_name);
	}
	exit(EXIT_FAILURE);
}

// https://llvm.org/docs/tutorial/MyFirstLanguageFrontend/LangImpl01.html
void lex(std::vector<Token> &tokens) {
	int linenum = 1;
	char c = input_file.get();
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

void parse_labels(const std::vector<Token> &tokens) {
	std::string curr_sect = "";

	for (int i = 0; i < tokens.size(); i++) {
		Token curr_token = tokens[i];
		if (curr_token.type == OTHER) {
			if (curr_token.text == ".section") {
				curr_sect = tokens[++i].text;
				if (!section_map.contains(curr_sect)) {
					section_map[curr_sect] = sections.size();
					sections.push_back({curr_sect, {}});
				}
			} else if (curr_token.text == "global") {
				const std::string &label = tokens[++i].text;
				if (label.starts_with('.'))
					fatal(curr_token.linenum, "local label in global directive");
				if (!symbols.contains(label))
					symbols[label] = {std::move(label), 0, true};
				else
					symbols[label].global = true;
			} else if (curr_token.text == "extern") {
				const std::string &label = tokens[++i].text;
				if (label.starts_with('.'))
					fatal(curr_token.linenum, "local label in extern directive");
				if (symbols.contains(label))
					fatal(curr_token.linenum, "duplicate definition of label '" + label + "`");
				symbols[label] = {std::move(label), 0, true};
			}
		} else if (curr_token.type == LABEL) {
			if (curr_token.text.starts_with('.')) {
				if (prev_label.empty())
					fatal(curr_token.linenum, "local label in global scope");
				symbols[curr_token.text] = {prev_label + curr_token.text, 0, false};
			} else {
				prev_label = curr_token.text;
				symbols[curr_token.text] = {std::move(curr_token.text), 0, false};
			}
		}
	}
}

void process_instructions() {
	// sect curr_sect = UNDEF;
	// size_t instr_cnt = 0;
	// for (size_t i = 0; i < lines.size(); i++) {
	// 	std::string line = lines[i];
	// 	while (line.size() == 0 && ++i < lines.size())
	// 		line = lines[i];
	// 	if (i == lines.size())
	// 		break;
	// 	if (line.starts_with("section ")) {
	// 		if (line == "section .text") {
	// 			curr_sect = TEXT;
	// 		} else if (line == "section .data") {
	// 			curr_sect = DATA;
	// 		} else if (line == "section .bss") {
	// 			curr_sect = BSS;
	// 		}
	// 	} else {
	// 		if (curr_sect == TEXT) {
	// 			// parse instruction
	// 			std::string instr = line.substr(0, line.find(' '));
	// 			if (instr.ends_with(':')) {
	// 				instr = instr.substr(0, instr.size() - 1);
	// 				if (instr[0] != '.') {
	// 					prev_label = instr.substr(0, instr.find('.'));
	// 					pad(16, text_buffer);
	// 				} else {
	// 					instr = prev_label + instr;
	// 				}
	// 				reloc_table[instr] = text_buffer.size();
	// 				continue;
	// 			} else {
	// 				for (size_t i = 0; i < instr.size(); i++)
	// 					instr[i] = tolower(instr[i]);
	// 			}
	// 			if (instr == "global") {
	// 				std::string label = line.substr(7);
	// 				if (label[0] == '.') {
	// 					std::cerr << "avertissement : " << input_name << ':' << i + 1 << ": étiquette locale dans une directive global" << std::endl;
	// 					label = prev_label + label;
	// 				}
	// 				global.insert(label);
	// 				continue;
	// 			} else if (instr == "extern") {
	// 				std::string label = line.substr(7);
	// 				if (label[0] == '.')
	// 					cerr(i + 1, "étiquette locale dans une directive extern");
	// 				extern_labels_map[label] = extern_labels.size();
	// 				extern_labels.push_back(label);
	// 				continue;
	// 			}
	// 			std::vector<std::string> args;
	// 			size_t pos = line.find(' ');
	// 			while (pos != std::string::npos) {
	// 				size_t next = line.find(',', pos + 1);
	// 				if (next == std::string::npos) {
	// 					next = line.size();
	// 					args.push_back(line.substr(pos + 1, next - pos - 1));
	// 					break;
	// 				}
	// 				args.push_back(line.substr(pos + 1, next - pos - 1));
	// 				pos = next;
	// 			}
	// 			if (instr[0] == 'd' && instr.size() == 2) {
	// 				parse_d(instr, args, i, text_buffer);
	// 			} else if (instr == "align") {
	// 				pad(std::stoi(args[0]), text_buffer);
	// 			} else {
	// 				handle(instr, args, i + 1, instr_cnt);
	// 			}
	// 			instr_cnt++;
	// 		}
	// 	}
	// }
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

	// Lex the input file
	std::vector<Token> tokens;
	lex(tokens);

	parse_labels();

	process_instructions();

	if (output_format == ELF)
		ELF_write(sections, symbols, output_name);
	else if (output_format == COFF)
		generate_coff(output, bss_size);

	return EXIT_SUCCESS;
}
