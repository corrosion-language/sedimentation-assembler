#include "elf.hpp"

#include <fstream>
#include <iostream>
#include <regex>

std::string input, output;
std::ifstream in;

std::regex extraneous_whitespace("(?: |\\t)+");

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

bool preprocess(std::string &line) {
	// Remove comments
	bool in_string = false;
	for (int i = 0; i < line.size(); i++) {
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
		return false;
	}
	line = line.substr(l, r - l + 1);

	// Remove extraneous whitespace
	for (int i = 0; i < line.size(); i++) {
		if (line[i] == '"' && (i == 0 || line[i - 1] != '\\')) {
			in_string = !in_string;
			continue;
		}
		if (in_string)
			continue;

		size_t pos = i;
		while (line[pos] == ' ' || line[pos] == '\t')
			pos++;
		if (pos > i + 1)
			line.erase(i, pos - i - 1);
	}

	return true;
}

int main(int argc, char **argv) {
	std::vector<Section> sections;
	std::vector<Symbol> symbols;

	// Parse arguments
	if (parse_args(argc, argv) != 0) {
		return 1;
	}

	in.open(input, std::ios::binary);
	std::string line;
	// Line by line parsing
	while (std::getline(in, line)) {
		// Preprocessing
		if (!preprocess(line))
			continue;

		std::cout << line << std::endl;
	}

	return 0;
}
