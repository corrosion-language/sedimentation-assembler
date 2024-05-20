#include "translate.hpp"

#include <unordered_map>

std::unordered_map<std::string, uint8_t> prefix_map{
	{"cs", 0x2e},	{"ds", 0x3e},  {"es", 0x26},   {"fs", 0x64},   {"gs", 0x65},	{"ss", 0x36},
	{"lock", 0xf0}, {"rep", 0xf3}, {"repe", 0xf3}, {"repz", 0xf3}, {"repne", 0xf2}, {"repnz", 0xf2},
};

void translate(const std::vector<std::string> &prefixes, const std::string &instr, const std::vector<std::string> &args, std::vector<uint8_t> &out) {
	// Encode prefixes
	for (const auto &prefix : prefixes) {
		auto it = prefix_map.find(prefix);
		if (it != prefix_map.end()) {
			out.push_back(it->second);
		} else {
			throw std::runtime_error("prefixe inconnue : " + prefix);
		}
	}
}
