#include "elf.hpp"

void rand_bytes(char *buf, size_t len) {
	std::ifstream f("/dev/urandom", std::ios::binary);
	f.read(buf, len);
	f.close();
}

void generate(std::ofstream &f, std::vector<uint8_t> &output_buffer, uint64_t data_size, uint64_t bss_size) {
	char *zero = new char[64];
	bzero(zero, 64);
	// elf ident
	f.write("\x7f\x45LF\x02\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00", 16);
	// elf header
	f.write("\x02\x00\x3e\x00\x01\x00\x00\x00", 8);
	// entry point
	uint64_t vaddr;
	rand_bytes((char *)&vaddr, 8);
	vaddr <<= 4;
	vaddr = 0x402000;
	uint64_t tmp = vaddr + (data_size + 15) / 16 * 16; // virtual address + data section (up to nearest 16)
	f.write((const char *)&tmp, 8);
	// program header position
	tmp = 64 + output_buffer.size(); // elf header, section content
	f.write((const char *)&tmp, 8);
	// section header position
	tmp = 64 + output_buffer.size() + 56 * (1 + !!data_size + !!bss_size); // elf header, section content, one program header
	f.write((const char *)&tmp, 8);
	// flags
	f.write(zero, 4);
	// header size
	tmp = 64;
	f.write((const char *)&tmp, 2);
	// size of entry in phtable
	tmp = 56;
	f.write((const char *)&tmp, 2);
	// number of entries in phtable
	tmp = 1 + !!data_size + !!bss_size;
	f.write((const char *)&tmp, 2);
	// size of entry in shtable
	tmp = 64;
	f.write((const char *)&tmp, 2);
	// number of entries in shtable
	tmp = 5 + !!data_size + !!bss_size;
	f.write((const char *)&tmp, 2);
	// index of section with section names
	tmp -= 3;
	f.write((const char *)&tmp, 2);
	// write from buffer
	for (auto &byte : output_buffer)
		f.write((const char *)&byte, 1);
	// program header
	// data segment
	if (data_size) {
		// type = load
		tmp = 1;
		f.write((const char *)&tmp, 4);
		// flags = read
		tmp = 0b100;
		f.write((const char *)&tmp, 4);
		// offset
		tmp = 64;
		f.write((const char *)&tmp, 8);
		// vaddr
		f.write((const char *)&vaddr, 8);
		// undef bytes
		f.write(zero, 8);
		// file size
		tmp = data_size;
		f.write((const char *)&tmp, 8);
		// mem size
		tmp = (data_size + 15) / 16 * 16;
		f.write((const char *)&tmp, 8);
		// align
		tmp = 16;
		f.write((const char *)&tmp, 8);
	}
	// text segment
	// type = load
	tmp = 1;
	f.write((const char *)&tmp, 4);
	// flags = read, execute
	tmp = 0b101;
	f.write((const char *)&tmp, 4);
	// offset
	tmp = 64 + data_size;
	f.write((const char *)&tmp, 8);
	// vaddr
	tmp = vaddr + (data_size + 15) / 16 * 16;
	f.write((const char *)&tmp, 8);
	// undef bytes
	f.write(zero, 8);
	// file size
	tmp = output_buffer.size() - data_size;
	f.write((const char *)&tmp, 8);
	// mem size
	tmp = (output_buffer.size() - data_size + 15) / 16 * 16;
	f.write((const char *)&tmp, 8);
	// align
	tmp = 16;
	f.write((const char *)&tmp, 8);
	// bss segment
	if (bss_size) {
		// type = load
		tmp = 1;
		f.write((const char *)&tmp, 4);
		// flags = read, write
		tmp = 0b110;
		f.write((const char *)&tmp, 4);
		// offset
		f.write(zero, 8);
		// vaddr
		tmp = vaddr + (data_size + 15) / 16 * 16 + (output_buffer.size() - data_size + 15) / 16 * 16;
		f.write((const char *)&tmp, 8);
		// undef bytes
		f.write(zero, 8);
		// file size
		f.write(zero, 8);
		// mem size
		tmp = (bss_size + 15) / 16 * 16;
		f.write((const char *)&tmp, 8);
		// align
		tmp = 16;
		f.write((const char *)&tmp, 8);
	}

	// section header
	// null section
	f.write(zero, 64);
	// data section
	if (data_size) {
		// name
		tmp = 1;
		f.write((const char *)&tmp, 4);
		// type = progbits
		tmp = 1;
		f.write((const char *)&tmp, 4);
		// flags = alloc
		tmp = 0x2;
		f.write((const char *)&tmp, 8);
		// addr
		f.write((const char *)&vaddr, 8);
		// offset
		tmp = 64;
		f.write((const char *)&tmp, 8);
		// size
		f.write((const char *)&data_size, 8);
		// link and info
		f.write(zero, 8);
		// align
		tmp = 4;
		f.write((const char *)&tmp, 8);
		// entry size
		f.write(zero, 8);
	}
	// text section
	// name
	tmp = 9;
	f.write((const char *)&tmp, 4);
	// type = progbits
	tmp = 1;
	f.write((const char *)&tmp, 4);
	// flags = alloc, execinstr
	tmp = 0x4 | 0x2;
	f.write((const char *)&tmp, 8);
	// addr
	tmp = vaddr + (data_size + 15) / 16 * 16;
	f.write((const char *)&tmp, 8);
	// offset
	tmp = 64 + data_size;
	f.write((const char *)&tmp, 8);
	// size
	tmp = output_buffer.size() - data_size;
	f.write((const char *)&tmp, 8);
	// link and info
	f.write(zero, 8);
	// align
	tmp = 16;
	f.write((const char *)&tmp, 8);
	// entry size
	f.write(zero, 8);
	// no bss because forgor :skull:
	if (bss_size) {
		// name
		tmp = 15;
		f.write((const char *)&tmp, 4);
		// type = progbits
		tmp = 1;
		f.write((const char *)&tmp, 4);
		// flags = alloc, execinstr
		tmp = 0x4 | 0x2;
		f.write((const char *)&tmp, 8);
		// addr
		tmp = vaddr + (data_size + 15) / 16 * 16;
		f.write((const char *)&tmp, 8);
		// offset
		tmp = 64 + data_size;
		f.write((const char *)&tmp, 8);
		// size
		tmp = output_buffer.size() - data_size;
		f.write((const char *)&tmp, 8);
		// link and info
		f.write(zero, 8);
		// align
		tmp = 4;
		f.write((const char *)&tmp, 8);
		// entry size
		f.write(zero, 8);
	}
	uint64_t i = (uint64_t)f.tellp() + 64 * 3;
	// shstrtab
	// name
	tmp = 20;
	f.write((const char *)&tmp, 4);
	// type = strtab
	tmp = 3;
	f.write((const char *)&tmp, 4);
	// flags
	tmp = 0x20;
	f.write((const char *)&tmp, 8);
	// addr
	f.write(zero, 8);
	// offset
	f.write((const char *)&i, 8);
	i += 46;
	// size
	tmp = 46;
	f.write((const char *)&tmp, 8);
	// link and info
	f.write(zero, 8);
	// align
	tmp = 1;
	f.write((const char *)&tmp, 8);
	// entry size
	f.write(zero, 8);
	// strtab
	// name
	tmp = 30;
	f.write((const char *)&tmp, 4);
	// type = strtab
	tmp = 3;
	f.write((const char *)&tmp, 4);
	// flags
	tmp = 0x20;
	f.write((const char *)&tmp, 8);
	// addr
	f.write(zero, 8);
	// offset
	f.write((const char *)&i, 8);
	i += 9;
	// size
	tmp = 9;
	f.write((const char *)&tmp, 8);
	// link and info
	f.write(zero, 8);
	// align
	tmp = 1;
	f.write((const char *)&tmp, 8);
	// entry size
	f.write(zero, 8);
	// symtab
	// name
	tmp = 38;
	f.write((const char *)&tmp, 4);
	// type = symtab
	tmp = 2;
	f.write((const char *)&tmp, 4);
	// flags
	f.write(zero, 8);
	// addr
	f.write(zero, 8);
	// offset
	f.write((const char *)&i, 8);
	// size
	tmp = 48;
	f.write((const char *)&tmp, 8);
	// link (index of strtab)
	tmp = 3 + !!data_size + !!bss_size;
	f.write((const char *)&tmp, 4);
	// info (index of last non-local symbol + 1)
	tmp = 2;
	f.write((const char *)&tmp, 4);
	// align
	tmp = 8;
	f.write((const char *)&tmp, 8);
	// entry size
	tmp = 24;
	f.write((const char *)&tmp, 8);
	// shstrtab data
	f.write("\0.rodata\0.text\0.bss\0.shstrtab\0.strtab\0.symtab\0", 46);
	// strtab data
	f.write("\0_start\0", 9);
	// symtab data
	// index 0 is undefined
	// index 1 is _start
	// zeros
	f.write(zero, 24);
	// name
	tmp = 1;
	f.write((const char *)&tmp, 4);
	// info = global,notype
	tmp = 0x10;
	f.write((const char *)&tmp, 1);
	// other = default
	f.write(zero, 1);
	// section = text
	tmp = 1 + !!data_size;
	f.write((const char *)&tmp, 2);
	// value
	tmp = vaddr + (data_size + 15) / 16 * 16;
	f.write((const char *)&tmp, 8);
	// size
	f.write(zero, 8);

	delete[] zero;
	std::cout << std::dec << f.tellp() << " bytes written.." << std::endl;
}
