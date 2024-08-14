#pragma once
#ifndef UTILITY_HPP
#define UTILITY_HPP

#include "defines.hpp"

#include <string>

short reg_num(const std::string &);
short reg_size(const std::string &);
short mem_size(const std::string &);
OperandType get_optype(const std::string &);
MemOperand *parse_mem(std::string, short &);
// -2 means resolved symbol
// -3 means relocatable symbol
// -4 means PLT reloc
std::pair<unsigned long long, short> parse_imm(std::string);

#endif
