#pragma once
#ifndef TRANSLATE_HPP
#define TRANSLATE_HPP

#include "main.hpp"
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

extern std::string input_name;
extern std::unordered_map<std::string, uint64_t> data_labels;
extern std::vector<std::string> text_labels;
extern std::unordered_map<std::string, int> text_labels_map;

enum op_type { INVALID, REG, MEM, IMM, OFF };
enum sect { UNDEF, TEXT, RODATA, BSS };

typedef bool (*handler)(std::vector<std::string> &, size_t);

bool handle(std::string &, std::vector<std::string> &, size_t);

#endif
