#pragma once
#ifndef UTILITY_HPP
#define UTILITY_HPP

#include "defines.hpp"

extern std::string prev_label;
extern std::unordered_map<std::string, std::pair<Section, size_t>> labels;

short reg_num(const std::string &);
short reg_size(const std::string &);
short mem_size(const std::string &);
OperandType get_optype(const std::string &);
MemOperand *parse_mem(std::string, short &);
std::pair<unsigned long long, short> parse_imm(std::string);

#endif
