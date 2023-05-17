#pragma once
#ifndef VEX_HPP
#define VEX_HPP

#include "defines.hpp"
#include "utility.hpp"
#include <iostream>
#include <string>
#include <vector>

extern void cerr(const int i, const std::string &s);

extern std::vector<std::string> text_labels;
extern std::unordered_map<std::string, size_t> text_labels_map;
extern std::vector<uint8_t> text_buffer;
extern std::vector<reloc_entry> relocations;
extern std::unordered_map<std::string, uint64_t> reloc_table;
extern std::string error;

void handle_vex(std::string &, std::vector<std::string> &, const size_t, const bool);

#endif
