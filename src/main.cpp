#include "defines.hpp"
#include "elf.hpp"
#include "translate.hpp"

#include <argp.h>
#include <cstring>
#include <fstream>
#include <iostream>

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
// Following variables are global and will be used with "extern" in other files
std::vector<RelocEntry> relocations;
std::vector<Section> sections;
std::unordered_map<std::string, uint16_t> section_map;
std::unordered_map<std::string, Symbol> symbols;
std::string prev_label; // Last label that was not a dot

[[noreturn]] void fatal(const int linenum, const std::string &&msg) {
	std::cerr << input_name << ":" << linenum << ": error: " << msg << std::endl;
	if (output_file.is_open()) {
		output_file.close();
		remove(output_name);
	}
	exit(EXIT_FAILURE);
}

void warning(const int linenum, const std::string &&msg) {
	std::cerr << input_name << ":" << linenum << ": warning: " << msg << std::endl;
}

// https://llvm.org/docs/tutorial/MyFirstLanguageFrontend/LangImpl01.html
void lex(std::vector<Token> &tokens) {
	int linenum = 1;
	char c = input_file.get();
	// Uses tokens.back() as a buffer to accumulate characters until a token is complete
	tokens.push_back({NEWLINE, "", 1});
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
		if (input_file.eof())
			break;

		// Comment
		if (c == ';') {
			while (c != '\n' && !input_file.eof())
				c = input_file.get();
			continue;
		}

		if (tokens.size() == 1 || tokens.rbegin()[1].type == NEWLINE) {
			// Beginning of line means must be either a label or an instruction/directive
			while (isalnum(c) || c == '_' || c == '#' || c == '@' || c == '~' || c == '?') {
				tokens.back().text += c;
				c = input_file.get();
			}
			if (c == ':')
				tokens.back().type = LABEL;
			else
				tokens.back().type = INSTR;
			if (c == ':')
				c = input_file.get();

			if (tokens.back().text.empty())
				fatal(linenum, "syntax error");
		} else if (tokens.rbegin()[1].type == LABEL || tokens.rbegin()[1].type == INSTR || tokens.rbegin()[1].type == ARG) {
			// Allows multiple prefixes
			if (tokens.rbegin()[1].type == INSTR || tokens.rbegin()[1].type == ARG) {
				Token &prev_token = tokens.rbegin()[1];
				if (!(prev_token.text == "lock" || prev_token.text == "repne" || prev_token.text == "repnz" || prev_token.text == "rep" ||
					  prev_token.text == "repe" || prev_token.text == "repz")) {
					// Must be an argument
					bool in_string = c == '"';
					while (!(c == ',' && !in_string) && c != '\n' && !input_file.eof()) {
						tokens.back().text += c;
						c = input_file.get();
						if (tokens.back().text.back() != '\\' && c == '"')
							in_string = !in_string;
					}
					if (in_string)
						fatal(linenum, "unterminated string");
					tokens.back().type = ARG;
					if (c == ',')
						c = input_file.get();

					if (tokens.back().text.empty())
						fatal(linenum, "syntax error");
					tokens.push_back({NEWLINE, "", linenum});
					continue;
				}
			}
			// Must be an instruction/directive
			while (isalnum(c)) {
				tokens.back().text += c;
				c = input_file.get();
			}
			tokens.back().type = INSTR;

			if (tokens.back().text.empty())
				fatal(linenum, "syntax error");
		}
		tokens.push_back({NEWLINE, "", linenum});
	}
	// Removes redundant newline tokens at the end
	tokens.pop_back();
	if (tokens.rbegin()[1].type == NEWLINE)
		tokens.pop_back();
}

void parse_labels(const std::vector<Token> &tokens) {
	std::string curr_sect = ""; // Current section

	for (int i = 0; i < tokens.size(); i++) {
		const Token &curr_token = tokens[i];
		if (curr_token.type == INSTR) {
			// Some type of directive
			if (curr_token.text == "section") {
				// Update current section
				curr_sect = tokens[++i].text;
				if (tokens[i].type != ARG)
					fatal(curr_token.linenum, "missing argument to section directive");

				// Create new section if it doesn't exist
				if (!section_map.count(curr_sect)) {
					section_map[curr_sect] = sections.size();
					sections.push_back({curr_sect, {}});
				}
			} else if (curr_token.text == "global") {
				const std::string &label = tokens[++i].text;
				if (tokens[i].type != ARG)
					fatal(curr_token.linenum, "missing argument to global directive");

				if (label[0] == '.')
					fatal(curr_token.linenum, "local label in global directive");

				// Create placeholder symbol if it doesn't exist (value = -1 - linenum)
				if (!symbols.count(label))
					symbols[label] = (Symbol){label, true, SYMTYPE_NONE, 0, -1 - curr_token.linenum};
				else
					symbols[label].global = true;
			} else if (curr_token.text == "extern") {
				const std::string &label = tokens[++i].text;
				if (tokens[i].type != ARG)
					fatal(curr_token.linenum, "missing argument to extern directive");

				if (label[0] == '.')
					fatal(curr_token.linenum, "local label in extern directive");
				if (symbols.count(label) && symbols[label].value > -2)
					fatal(curr_token.linenum, "duplicate definition of label `" + label + "'");
				symbols[label] = (Symbol){label, false, SYMTYPE_NONE, section_map[curr_sect] + 1, 0};
			}
		} else if (curr_token.type == LABEL) {
			// Label definition
			if (curr_token.text[0] == '.') {
				if (prev_label.empty())
					fatal(curr_token.linenum, "local label in global scope");
				if (symbols.count(prev_label + curr_token.text))
					fatal(curr_token.linenum, "duplicate definition of label `" + prev_label + curr_token.text + "'");
				symbols[prev_label + curr_token.text] = (Symbol){prev_label + curr_token.text, false, SYMTYPE_NONE, section_map[curr_sect] + 1, -1};
			} else {
				if (symbols.count(curr_token.text)) {
					if (symbols[curr_token.text].value > -2)
						fatal(curr_token.linenum, "duplicate definition of label `" + curr_token.text + "'");
					// If symbol is a placeholder, only update section index and value (everything else is already set)
					symbols[curr_token.text].shndx = section_map[curr_sect] + 1;
					symbols[curr_token.text].value = -1;
				} else {
					symbols[curr_token.text] = (Symbol){curr_token.text, false, SYMTYPE_NONE, section_map[curr_sect] + 1, -1};
				}
				prev_label = curr_token.text;
			}
		}
	}

	// Verify that all symbols are defined
	for (auto &sym : symbols) {
		if (sym.second.value <= -2)
			fatal(-1 - sym.second.value, "undefined reference to label `" + sym.first + "'");
	}
}

// Returns new index
int parse_escape(const std::string &str, int i, std::vector<uint8_t> &output_buffer, const int linenum, bool is_string = true) {
	ssize_t res = -1;
	switch (str[i + 1]) {
	case '\'':
		output_buffer.push_back('\'');
		break;
	case '"':
		output_buffer.push_back('"');
		break;
	case '?':
		output_buffer.push_back('?');
		break;
	case '\\':
		output_buffer.push_back('\\');
		break;
	case 'a':
		output_buffer.push_back('\a');
		break;
	case 'b':
		output_buffer.push_back('\b');
		break;
	case 'f':
		output_buffer.push_back('\f');
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
	case 'v':
		output_buffer.push_back('\v');
		break;
	case 'x':
		output_buffer.push_back(std::stoi(str.substr(i + 2, 2), nullptr, 16));
		i += 2;
		break;
	case 'u':
		res = std::stoul(str.substr(i + 2, 4), nullptr, 16);
		i -= 4;
		[[fallthrough]];
	case 'U':
		if (!is_string && res >= 0x80)
			fatal(linenum, "cannot use Unicode escape sequence in character literal");
		if (res == -1)
			res = std::stoul(str.substr(i + 2, 8), nullptr, 16);
		if (res < 0x80) {
			output_buffer.push_back(res);
		} else if (res < 0x800) {
			output_buffer.push_back(0xc0 | (res >> 6));
			output_buffer.push_back(0x80 | (res & 0x3f));
		} else if (res < 0x10000) {
			output_buffer.push_back(0xe0 | (res >> 12));
			output_buffer.push_back(0x80 | ((res >> 6) & 0x3f));
			output_buffer.push_back(0x80 | (res & 0x3f));
		} else {
			output_buffer.push_back(0xf0 | (res >> 18));
			output_buffer.push_back(0x80 | ((res >> 12) & 0x3f));
			output_buffer.push_back(0x80 | ((res >> 6) & 0x3f));
			output_buffer.push_back(0x80 | (res & 0x3f));
		}
		i += 8;
		break;
	default:
		fatal(linenum, "invalid escape sequence `\\" + str[i + 1] + '\'');
	}
	i++;
	return i;
}

// Parse define directives (db, dw, dd, dq)
void parse_d(const std::string &instr, const std::vector<std::string> &args, const int linenum, std::vector<uint8_t> &output_buffer) {
	for (int i = 0; i < args.size(); i++) {
		if (args[i][0] == '"') {
			for (int j = 1; j < args[i].size() - 1; j++) {
				if (args[i][j] == '\\') {
					try {
						j = parse_escape(args[i], j, output_buffer, linenum);
					} catch (std::invalid_argument &e) {
						fatal(linenum, "invalid numeric escape sequence");
					}
				} else {
					output_buffer.push_back(args[i][j]);
				}
			}
			continue;
		}
		try {
			if (args[i][0] == '\'') {
				if (args[i][1] == '\\') {
					try {
						parse_escape(args[i], 0, output_buffer, linenum, false);
					} catch (std::invalid_argument &e) {
						fatal(linenum, "invalid numeric escape sequence");
					}
				} else {
					output_buffer.push_back(args[i][1]);
				}
			} else if (instr == "db") {
				if (args[i][0] == '-' || args[i][0] == '+') {
					int32_t val = std::stoi(args[i], nullptr, 0);
					if (val < -128)
						fatal(linenum, "signed integer overflow is undefined");
					output_buffer.push_back(val);
				} else {
					uint32_t val = std::stoul(args[i], nullptr, 0);
					if (val > 0xff)
						warning(linenum, "integer is too large and will be truncated");
					output_buffer.push_back(val & 0xff);
				}
			} else if (instr == "dw") {
				if (args[i][0] == '-' || args[i][0] == '+') {
					int32_t val = std::stoi(args[i], nullptr, 0);
					if (val < -32768)
						fatal(linenum, "signed integer overflow is undefined");
					output_buffer.push_back(val & 0xff);
					output_buffer.push_back((val >> 8) & 0xff);
				} else {
					uint32_t val = std::stoul(args[i], nullptr, 0);
					if (val > 0xffff)
						warning(linenum, "integer is too large and will be truncated");
					output_buffer.push_back(val & 0xff);
					output_buffer.push_back((val >> 8) & 0xff);
				}
			} else if (instr == "dd") {
				if (args[i][0] == '-' || args[i][0] == '+') {
					int64_t val = std::stoll(args[i], nullptr, 0);
					if (val < -2147483648)
						fatal(linenum, "signed integer overflow is undefined");
					output_buffer.push_back(val & 0xff);
					output_buffer.push_back((val >> 8) & 0xff);
					output_buffer.push_back((val >> 16) & 0xff);
					output_buffer.push_back((val >> 24) & 0xff);
				} else {
					uint64_t val = std::stoull(args[i], nullptr, 0);
					if (val > 0xffffffff)
						warning(linenum, "integer is too large and will be truncated");
					output_buffer.push_back(val & 0xff);
					output_buffer.push_back((val >> 8) & 0xff);
					output_buffer.push_back((val >> 16) & 0xff);
					output_buffer.push_back((val >> 24) & 0xff);
				}
			} else if (instr == "dq") {
				if (args[i][0] == '-' || args[i][0] == '+') {
					int64_t val = std::stoll(args[i], nullptr, 0);
					output_buffer.push_back(val & 0xff);
					output_buffer.push_back((val >> 8) & 0xff);
					output_buffer.push_back((val >> 16) & 0xff);
					output_buffer.push_back((val >> 24) & 0xff);
					output_buffer.push_back((val >> 32) & 0xff);
					output_buffer.push_back((val >> 40) & 0xff);
					output_buffer.push_back((val >> 48) & 0xff);
					output_buffer.push_back((val >> 56) & 0xff);
				} else {
					uint64_t val = std::stoull(args[i], nullptr, 0);
					output_buffer.push_back(val & 0xff);
					output_buffer.push_back((val >> 8) & 0xff);
					output_buffer.push_back((val >> 16) & 0xff);
					output_buffer.push_back((val >> 24) & 0xff);
					output_buffer.push_back((val >> 32) & 0xff);
					output_buffer.push_back((val >> 40) & 0xff);
					output_buffer.push_back((val >> 48) & 0xff);
					output_buffer.push_back((val >> 56) & 0xff);
				}
			} else {
				fatal(linenum, "unknown directive" + instr + "'");
			}
		} catch (std::invalid_argument &e) {
			fatal(linenum, "invalid number");
		} catch (std::out_of_range &e) {
			fatal(linenum, "number is too large");
		}
	}
}

void pad(unsigned int align, std::vector<uint8_t> &output_buffer) {
	if (align == 0)
		fatal(0, "alignment must be greater than zero");
	int pad = output_buffer.size() % align;
	if (pad == 0)
		return;
	pad = align - pad;
	while (pad >= 11) {
		output_buffer.insert(output_buffer.end(), {0x66, 0x66, 0x66, 0x0f, 0x1f, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00});
		pad -= 11;
	}
	if (pad == 1)
		output_buffer.insert(output_buffer.end(), {0x90});
	else if (pad == 2)
		output_buffer.insert(output_buffer.end(), {0x66, 0x90});
	else if (pad == 3)
		output_buffer.insert(output_buffer.end(), {0x0f, 0x1f, 0x00});
	else if (pad == 4)
		output_buffer.insert(output_buffer.end(), {0x0f, 0x1f, 0x40, 0x00});
	else if (pad == 5)
		output_buffer.insert(output_buffer.end(), {0x0f, 0x1f, 0x44, 0x00, 0x00});
	else if (pad == 6)
		output_buffer.insert(output_buffer.end(), {0x66, 0x0f, 0x1f, 0x44, 0x00, 0x00});
	else if (pad == 7)
		output_buffer.insert(output_buffer.end(), {0x0f, 0x1f, 0x80, 0x00, 0x00, 0x00, 0x00});
	else if (pad == 8)
		output_buffer.insert(output_buffer.end(), {0x0f, 0x1f, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00});
	else if (pad == 9)
		output_buffer.insert(output_buffer.end(), {0x66, 0x0f, 0x1f, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00});
	else if (pad == 10)
		output_buffer.insert(output_buffer.end(), {0x66, 0x66, 0x0f, 0x1f, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00});
}

void process_instructions(const std::vector<Token> &tokens) {
	std::string curr_sect = "";
	prev_label = "";

	for (int i = 0; i < tokens.size(); i++) {
		const Token &curr_token = tokens[i];
		if (curr_token.type == INSTR) {
			const std::string &instr = curr_token.text;
			int linenum = curr_token.linenum;
			if (instr == "section") {
				curr_sect = tokens[++i].text;
				i++;
			} else if (instr == "global" || instr == "extern") {
				// Skip because already processed in parse_labels
				i += 2;
			} else if (instr == "align") {
				if (tokens[++i].type != ARG)
					fatal(linenum, "missing argument to align directive");

				try {
					pad(std::stoul(tokens[i].text), sections[section_map[curr_sect]].data);
				} catch (std::invalid_argument &e) {
					fatal(linenum, "invalid argument to align directive");
				}
			} else if (instr.starts_with("res") && instr.size() == 4 && instr.find_first_of("bwdqo") != 3) {
				try {
					if (tokens[++i].type != ARG)
						fatal(linenum, "missing argument to " + instr + " directive");
					unsigned int size = std::stoul(tokens[i].text);
					if (instr[3] == 'b')
						sections[section_map[curr_sect]].data.resize(sections[section_map[curr_sect]].data.size() + size);
					else if (instr[3] == 'w')
						sections[section_map[curr_sect]].data.resize(sections[section_map[curr_sect]].data.size() + size * 2);
					else if (instr[3] == 'd')
						sections[section_map[curr_sect]].data.resize(sections[section_map[curr_sect]].data.size() + size * 4);
					else if (instr[3] == 'q')
						sections[section_map[curr_sect]].data.resize(sections[section_map[curr_sect]].data.size() + size * 8);
					else
						sections[section_map[curr_sect]].data.resize(sections[section_map[curr_sect]].data.size() + size * 16);
				} catch (std::invalid_argument &e) {
					fatal(linenum, "invalid argument to " + instr + " directive");
				}
				i++;
			} else {
				// Parse arguments
				std::vector<std::string> args;
				while (tokens[++i].type != NEWLINE)
					args.push_back(tokens[i].text);

				// Define directives
				if (instr[0] == 'd' && instr.size() == 2)
					parse_d(instr, args, linenum, sections[section_map[curr_sect]].data);
				// else
				// 	handle_instruction(curr_token.text, args, linenum, sections[section_map[curr_sect]].data);
			}
		} else if (curr_token.type == LABEL) {
			// Update symbol value
			if (curr_token.text[0] == '.') {
				symbols[prev_label + curr_token.text].value = sections[section_map[curr_sect]].data.size();
			} else {
				symbols[curr_token.text].value = sections[section_map[curr_sect]].data.size();
				prev_label = curr_token.text;
			}
			// Bypass the newline check because there can be multiple labels or a label and an instruction on the same line
			continue;
		}
		// Make sure there's a newline after each instruction
		if (tokens[i].type != NEWLINE)
			fatal(curr_token.linenum, "syntax error");
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
