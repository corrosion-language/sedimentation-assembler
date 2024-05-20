#include "elf.hpp"

#include <fstream>
#include <iostream>
#include <regex>

std::string input, output;
std::ifstream in;

std::regex extraneous_whitespace("(?: |\\t)+b"); // one or more spaces or tabs in a row
std::regex label("([a-zA-Z_.][a-zA-Z0-9_.]*):"); // letter, underscore, or dot followed by letters, numbers, underscores, or dots, ending with a colon

/// TODO: Fix argument parsing, this is just a placeholder
int parse_args(int argc, char **argv) {
	if (argc < 2) {
		std::cout << "Usage: " << argv[0] << " <input> [output]" << std::endl;
		return 1;
	}
	if (argc == 2) {
		input = argv[1];
		if (input.find_last_of('.') == std::string::npos) {
			output = input + ".o";
		} else {
			output = input.substr(0, input.find_last_of('.')) + ".o";
		}
	}

	return 0;
}

void preprocess(std::string &line) {
	// Remove comments
	bool in_string = false;
	for (size_t i = 0; i < line.size(); i++) {
		if (line[i] == '"' && (i == 0 || line[i - 1] != '\\')) {
			in_string = !in_string;
			continue;
		}
		if (in_string)
			continue;
		if (line[i] == ';') {
			line = line.substr(0, i);
			break;
		}
	}
	if (in_string)
		throw "Unterminated string";

	// Remove leading and trailing whitespace
	size_t l = line.find_first_not_of(" \t");
	size_t r = line.find_last_not_of(" \t");
	if (l == std::string::npos || r == std::string::npos) {
		line = "";
		return;
	}
	line = line.substr(l, r - l + 1);

	// Remove extraneous whitespace
	/// TODO: See if this is necessary or if it can be done in the lexer
	for (size_t i = 0; i < line.size(); i++) {
		if (line[i] == '"' && (i == 0 || line[i - 1] != '\\')) {
			in_string = !in_string;
			continue;
		}
		if (in_string)
			continue;

		size_t pos = i;
		while (line[pos] == ' ' || line[pos] == '\t')
			pos++;
		if (pos != i) {
			line.erase(i, pos - i - 1);
			line[i] = ' ';
		}
	}
}

int main(int argc, char **argv) {
	std::vector<Section> sections;
	std::vector<Symbol> symbols;

	// Parse arguments
	if (parse_args(argc, argv) != 0) {
		return 1;
	}

	// Load the input file into an array of lines
	in.open(input, std::ios::binary);
	if (!in.is_open()) {
		std::cerr << "impossible d'ouvrir le fichier d'entrée '" << input << '\'' << std::endl;
		return 1;
	}
	std::vector<std::string> lines;
	std::string line;
	while (std::getline(in, line)) {
		preprocess(line);
		lines.push_back(line);
	}
	in.close();

	return 0;
}
