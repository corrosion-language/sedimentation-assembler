#pragma once
#ifndef MAIN_HPP
#define MAIN_HPP

#include "elf.hpp"
#include "translate.hpp"
#include <chrono>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <set>
#include <sys/stat.h>
#include <unordered_map>
#include <vector>

#define profile(e)                                                                                                                                             \
	{                                                                                                                                                          \
		auto start = std::chrono::high_resolution_clock::now();                                                                                                \
		e;                                                                                                                                                     \
		auto end = std::chrono::high_resolution_clock::now();                                                                                                  \
		cum += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();                                                                      \
		num++;                                                                                                                                                 \
	}

enum sect { UNDEF, TEXT, DATA, BSS };

void cerr(const int i, const std::string &s);

#endif
