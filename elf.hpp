#include <string>
#include <vector>

struct Section {
	std::vector<uint8_t> data;
	std::string name;
	size_t size = 0; // Optional
};

struct Symbol {
	std::string name;
	uint64_t value;
	uint8_t type;
	bool global;
};

void ELF_Write(const std::vector<Section> &sections, const std::vector<Symbol> &symbols, const std::string &filename);
