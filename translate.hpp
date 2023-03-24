#pragma once
#ifndef TRANSLATE_HPP
#define TRANSLATE_HPP

#include "main.hpp"
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

extern const char *input_name;
extern std::unordered_map<std::string, uint64_t> data_labels;
extern std::vector<std::string> text_labels;
extern std::unordered_map<std::string, uint8_t> text_labels_map;
extern std::vector<uint8_t> output_buffer;
extern std::vector<std::pair<uint32_t, short>> relocations;
extern std::string prev_label;

enum op_type { INVALID, REG, MEM, IMM, OFF };
enum sect { UNDEF, TEXT, RODATA, BSS };

typedef bool (*handler)(std::vector<std::string> &);

bool handle(std::string &, std::vector<std::string> &, size_t);

#endif
