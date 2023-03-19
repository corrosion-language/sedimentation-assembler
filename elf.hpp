#pragma once
#ifndef ELF_HPP
#define ELF_HPP

#include <stdint.h>

struct program_header {
	uint32_t type, flags;
	uint64_t offset, vaddr, paddr, filesz, memsz, align;
};

struct section_header {
	uint32_t name, type;
	uint64_t flags, addr, offset, size;
	uint32_t link, info;
	uint64_t addralign, entsize;
};

#endif
