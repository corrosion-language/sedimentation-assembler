#pragma once
#ifndef MAIN_HPP
#define MAIN_HPP

#include "coff.hpp"
#include "elf.hpp"
#include "translate.hpp"

/*#define profile(e) \
	{                                                                                                                                                          \
		auto start = std::chrono::high_resolution_clock::now();                                                                                                \
		e;                                                                                                                                                     \
		auto end = std::chrono::high_resolution_clock::now();                                                                                                  \
		cum += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();                                                                      \
		num++;                                                                                                                                                 \
	}*/

void cerr(const int i, const std::string &s);

#endif
