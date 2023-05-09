#include "defines.hpp"
#include <regex>
#include <string>
#include <unordered_map>

extern std::string prev_label;
extern std::unordered_map<std::string, uint64_t> data_labels;
extern std::unordered_map<std::string, uint64_t> bss_labels;
extern std::unordered_map<std::string, size_t> extern_labels_map;
extern std::unordered_map<std::string, size_t> text_labels_map;
extern std::unordered_map<std::string, std::pair<sect, size_t>> labels;

short reg_num(const std::string &);
short reg_size(const std::string &);
short mem_size(const std::string &);
op_type get_optype(const std::string &);
mem_output *parse_mem(std::string, short &);
std::pair<unsigned long long, short> parse_imm(std::string);
