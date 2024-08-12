#pragma once
#ifndef TRANSLATE_HPP
#define TRANSLATE_HPP

#include "defines.hpp"
#include "utility.hpp"

extern void fatal(const int i, const std::string &s);

extern std::vector<RelocEntry> relocations;
extern std::string error;

void handle(std::string, std::vector<std::string>, const size_t, const size_t);

#endif
