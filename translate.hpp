#pragma once
#ifndef TRANSLATE_HPP
#define TRANSLATE_HPP

#include "main.hpp"
#include "utility.hpp"
#include <iostream>
#include <string>
#include <vector>

extern const char *input_name;
extern std::vector<std::string> text_labels;
extern std::unordered_map<std::string, size_t> text_labels_map;
extern std::vector<uint8_t> output_buffer;
extern std::vector<uint32_t> text_relocations;
extern std::vector<size_t> data_relocations;
extern std::vector<size_t> bss_relocations;
extern std::string prev_label;

typedef bool (*handler)(std::vector<std::string> &);

bool handle(std::string &, std::vector<std::string> &, size_t);

#endif
