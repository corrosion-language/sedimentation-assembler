#include "error.hpp"

#include <string>
#include <vector>

/**
 * @brief Translate an instruction into machine code, handles VEX coded instructions
 * @throws std::runtime_error containing an error message intended to be displayed to the user
 *
 * @param prefixes Prefixes
 * @param instr Instruction
 * @param args Arguments
 * @param out Output buffer
 */
void translate(const std::vector<std::string> &, const std::string &, const std::vector<std::string> &, std::vector<uint8_t> &);
