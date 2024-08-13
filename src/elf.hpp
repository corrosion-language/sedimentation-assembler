#pragma once
#ifndef ELF_HPP
#define ELF_HPP

#include "defines.hpp"

#include <fstream>
#include <vector>

void ELF_write(const std::vector<Section> &, const std::vector<Symbol> &, std::ofstream &);

#endif
