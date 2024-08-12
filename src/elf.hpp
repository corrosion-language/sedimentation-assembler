#pragma once
#ifndef ELF_HPP
#define ELF_HPP

#include "defines.hpp"

extern std::vector<std::string> extern_labels;
extern std::unordered_set<std::string> global;
extern std::vector<struct RelocEntry> relocations;
extern std::unordered_map<std::string, std::pair<Section, size_t>> labels;
extern std::string text_buffer;
extern std::string data_buffer;
extern std::string rodata_buffer;

void ELF_write(const std::vector<Section> &sections, const std::vector<Symbol> &symbols, const std::string &filename);

#endif
