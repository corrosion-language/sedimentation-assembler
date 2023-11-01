#include <string>
#include <vector>

struct Section {
	std::vector<uint8_t> data;
	std::string name;
};

struct Symbol {
	std::string name;
	uint64_t value;
	uint8_t type;
	bool global;
};

void ELF_Write(std::vector<Section> sections, std::vector<Symbol> symbols, std::string filename);
