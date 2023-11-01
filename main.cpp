#include "elf.hpp"

int main() {
	std::vector<Section> sections;
	std::vector<Symbol> symbols;

	// Create a section
	Section section;
	section.name = ".text";
	section.data = {0x48, 0x89, 0xf8, 0x48, 0x83, 0xc0, 0x01, 0xc3};
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
