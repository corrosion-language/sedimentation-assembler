#include "elf.hpp"

int main() {
	std::vector<Section> sections;
	std::vector<Symbol> symbols;

	// Create a section
	Section section;
	section.name = ".text";
	section.data = {'H', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd', '!'};
	sections.push_back(section);
	section.name = ".data";
	section.data = {'d', 'a', 't', 'a'};
	sections.push_back(section);

	// Create a symbol
	Symbol symbol;
	symbol.name = "main";
	symbol.value = 0;
	symbol.type = STT_FUNC;
	symbol.global = true;
	symbols.push_back(symbol);

	// Write the ELF file
	ELF_Write(sections, symbols, "a.out");

	return 0;
}
