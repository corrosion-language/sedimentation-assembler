#pragma once
#ifndef TRANSLATE_HPP
#define TRANSLATE_HPP

#include <string>
#include <unordered_map>
#include <vector>

typedef bool (*handler)(std::vector<std::string>);

// instruction, handler
std::unordered_map<std::string, handler> handlers{};

#endif
