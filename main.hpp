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
#include <sys/stat.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#define profile(e)                                                                                                                                             \
	{                                                                                                                                                          \
		auto start = std::chrono::high_resolution_clock::now();                                                                                                \
		e;                                                                                                                                                     \
		auto end = std::chrono::high_resolution_clock::now();                                                                                                  \
		cum += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();                                                                      \
		num++;                                                                                                                                                 \
	}

#endif
