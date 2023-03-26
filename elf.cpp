#include "elf.hpp"

void rand_bytes(char *buf, size_t len) {
	std::ifstream f("/dev/urandom", std::ios::binary);
	f.read(buf, len);
	f.close();
}

void generate_elf(std::ofstream &f, std::vector<uint8_t> &output_buffer, uint64_t data_size, uint64_t bss_size) {
	// structure:
	//  ELF header
	//  program headers
	//  section headers
	//  section data
	//  section names

	// virtual address of the first section
	uint64_t vaddr;
	rand_bytes((char *)&vaddr + 2, 2);
	vaddr &= 0x7fff0000;

	// page size
	uint64_t page_size = getpagesize();

	// list of symbols to include
	uint64_t strtab_size = 1;
	for (auto &s : reloc_table) {
		strtab_size += s.first.size() + 1;
	}

	// ELF header
	elf_header ehdr;
	memcpy(ehdr.ident, "\x7f\x45LF\x02\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00", 16);
	ehdr.type = 2; // executable
	ehdr.machine = 0x3e; // x86-64
	ehdr.version = 1; // current
	ehdr.entry = vaddr + 0x1000 + reloc_table.at("_start") - data_size;
	ehdr.phoff = sizeof(elf_header);
	ehdr.shoff = (2 + !!data_size + !!bss_size) * sizeof(program_header) + sizeof(elf_header);
	ehdr.flags = 0;
	ehdr.ehsize = sizeof(elf_header);
	ehdr.phentsize = sizeof(program_header);
	ehdr.phnum = 2 + !!data_size + !!bss_size; // 2 for null, text
	ehdr.shentsize = sizeof(section_header);
	ehdr.shnum = 5 + !!data_size + !!bss_size; // 3 for null, text, symtab, strtab, shstrtab
	ehdr.shstrndx = ehdr.shnum - 1; // no section names
	f.write((const char *)&ehdr, sizeof(elf_header));

	// program headers
	program_header phdr;
	// null
	phdr.type = 1; // loadable
	phdr.flags = 0b100; // read
	phdr.offset = 0;
	phdr.vaddr = vaddr;
	phdr.paddr = phdr.vaddr;
	phdr.filesz = ehdr.shoff;
	phdr.memsz = ehdr.shoff;
	phdr.align = page_size;
	f.write((const char *)&phdr, sizeof(program_header));

	// .text
	phdr.type = 1; // loadable
	phdr.flags = 0b101; // read, execute
	phdr.offset = ((736 + (reloc_table.size() + 1) * sizeof(symbol) + strtab_size + page_size - 1) & ~(page_size - 1));
	phdr.vaddr = phdr.vaddr + 0x1000; // align to page size
	phdr.paddr = phdr.vaddr;
	phdr.filesz = output_buffer.size() - data_size;
	phdr.memsz = phdr.filesz;
	phdr.align = page_size;
	f.write((const char *)&phdr, sizeof(program_header));

	// .rodata
	if (data_size) {
		phdr.type = 1; // loadable
		phdr.flags = 0b100; // read
		phdr.offset = phdr.offset + ((phdr.filesz + page_size - 1) & ~(page_size - 1));
		phdr.vaddr = phdr.vaddr + ((phdr.memsz + page_size - 1) & ~(page_size - 1));
		phdr.paddr = phdr.vaddr;
		phdr.filesz = data_size;
		phdr.memsz = data_size;
		phdr.align = page_size;
		f.write((const char *)&phdr, sizeof(program_header));
	}

	// .bss
	if (bss_size) {
		phdr.type = 1; // loadable
		phdr.flags = 0b110; // read, write
		phdr.offset = 0;
		phdr.vaddr = phdr.vaddr + ((phdr.memsz + page_size - 1) & ~(page_size - 1)); // align to page size
		phdr.paddr = phdr.vaddr;
		phdr.filesz = 0;
		phdr.memsz = bss_size;
		phdr.align = page_size;
		f.write((const char *)&phdr, sizeof(program_header));
	}

	// section headers
	section_header shdr;
	// null section
	bzero(&shdr, sizeof(section_header));
	f.write((const char *)&shdr, sizeof(section_header));

	// text section
	shdr.name = 1;
	shdr.type = 1; // progbits
	shdr.flags = 0x2 | 0x4; // alloc, execinstr
	shdr.addr = vaddr + 0x1000;
	shdr.offset = 0x1000;
	shdr.size = output_buffer.size() - data_size;
	shdr.link = 0;
	shdr.info = 0;
	shdr.addralign = 16;
	shdr.entsize = 0;
	f.write((const char *)&shdr, sizeof(section_header));

	// rodata section
	if (data_size) {
		shdr.name = 7;
		shdr.type = 1; // progbits
		shdr.flags = 0x2; // alloc
		shdr.addr = shdr.addr + ((shdr.size + page_size - 1) & ~(page_size - 1));
		shdr.offset = shdr.offset + ((data_size + page_size - 1) & ~(page_size - 1));
		shdr.size = data_size;
		shdr.link = 0;
		shdr.info = 0;
		shdr.addralign = 4;
		shdr.entsize = 0;
		f.write((const char *)&shdr, sizeof(section_header));

		// perform data relocations
		for (size_t &r : data_relocations) {
			uint32_t *p = (uint32_t *)(output_buffer.data() + r);
			*p += shdr.addr;
		}
	}

	// bss section
	if (bss_size) {
		shdr.name = 15;
		shdr.type = 8; // nobits
		shdr.flags = 0x2 | 0x1; // alloc, write
		shdr.addr = shdr.addr + ((shdr.size + page_size - 1) & ~(page_size - 1));
		shdr.offset = 0;
		shdr.size = bss_size;
		shdr.link = 0;
		shdr.info = 0;
		shdr.addralign = 1;
		shdr.entsize = 0;
		f.write((const char *)&shdr, sizeof(section_header));

		// perform bss relocations
		for (size_t &r : bss_relocations) {
			uint32_t *p = (uint32_t *)(output_buffer.data() + r);
			*p += shdr.addr;
		}
	}

	// symbol table
	shdr.name = 20;
	shdr.type = 2; // symtab
	shdr.flags = 0;
	shdr.addr = 0;
	shdr.offset = ehdr.shoff + ehdr.shentsize * ehdr.shnum;
	shdr.size = (reloc_table.size() + 1) * sizeof(symbol);
	shdr.link = ehdr.shnum - 2; // strtab
	shdr.info = reloc_table.size() + 1; // index of last non-local symbol + 1
	shdr.addralign = 8;
	shdr.entsize = sizeof(symbol);
	f.write((const char *)&shdr, sizeof(section_header));

	// strtab section
	shdr.name = 28;
	shdr.type = 3; // strtab
	shdr.flags = 0;
	shdr.addr = 0;
	shdr.offset = shdr.offset + shdr.size;
	shdr.size = strtab_size;
	shdr.link = 0;
	shdr.info = 0;
	shdr.addralign = 1;
	shdr.entsize = 0;
	f.write((const char *)&shdr, sizeof(section_header));

	// shstrtab section
	shdr.name = 36;
	shdr.type = 3; // strtab
	shdr.flags = 0;
	shdr.addr = 0;
	shdr.offset = shdr.offset + shdr.size;
	shdr.size = 46;
	shdr.link = 0;
	shdr.info = 0;
	shdr.addralign = 1;
	shdr.entsize = 0;
	f.write((const char *)&shdr, sizeof(section_header));

	uint64_t i = 1;
	symbol sym;
	// null symbol
	bzero(&sym, sizeof(symbol));
	f.write((const char *)&sym, sizeof(symbol));
	// write symtab
	for (auto s : reloc_table) {
		sym.name = i;
		i += s.first.size() + 1;
		sym.info = (global_syms.find(s.first) != global_syms.end()) << 4; // global
		sym.other = 0;
		sym.shndx = 1; // text
		sym.value = vaddr + 0x1000 + s.second - data_size;
		sym.size = 0;
		f.write((const char *)&sym, sizeof(symbol));
	}

	// write strtab
	f.seekp(f.tellp() + (std::streamoff)1);
	for (auto &s : reloc_table)
		f.write(s.first.c_str(), s.first.size() + 1);

	// write shstrtab
	f.write("\0.text\0.rodata\0.bss\0.symtab\0.strtab\0.shstrtab", 46);

	// seek to multiple of page size
	f.seekp(((size_t)f.tellp() + page_size - 1) & ~(page_size - 1));

	// write text
	f.write((const char *)output_buffer.data() + data_size, output_buffer.size() - data_size);

	// seek to multiple of page size
	f.seekp(((size_t)f.tellp() + page_size - 1) & ~(page_size - 1));

	// write data
	f.write((const char *)output_buffer.data(), data_size);

	// close file
	f.close();
}
