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
	//  symbol table
	//  string table
	//  symbol names

	// virtual address of the first section
	uint64_t vaddr = 0x400000;

	// ELF header
	elf_header ehdr;
	memcpy(ehdr.ident, "\x7f\x45LF\x02\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00", 16);
	ehdr.type = 2; // executable
	ehdr.machine = 0x3e; // x86-64
	ehdr.version = 1; // current
	ehdr.entry = vaddr + 0x1000; // beginning of second segment (first segment cannot be larger than 0x1000)
	ehdr.phoff = sizeof(elf_header);
	ehdr.shoff = (2 + !!data_size + !!bss_size) * sizeof(program_header) + sizeof(elf_header);
	ehdr.flags = 0;
	ehdr.ehsize = sizeof(elf_header);
	ehdr.phentsize = sizeof(program_header);
	ehdr.phnum = 2 + !!data_size + !!bss_size;
	ehdr.shentsize = sizeof(section_header);
	ehdr.shnum = 5 + !!data_size + !!bss_size;
	ehdr.shstrndx = 2 + !!data_size + !!bss_size; // third one from end
	f.write((const char *)&ehdr, sizeof(elf_header));

	// program headers
	// first segment (idk what this is but it's required i think)
	program_header phdr;
	phdr.type = 1; // loadable
	phdr.flags = 0b100; // read
	phdr.offset = 0;
	phdr.vaddr = vaddr;
	phdr.paddr = vaddr;
	phdr.filesz = (2 + !!data_size + !!bss_size) * sizeof(program_header) + sizeof(elf_header); // size of elf header + program headers
	phdr.memsz = phdr.filesz;
	phdr.align = 0x1000;
	f.write((const char *)&phdr, sizeof(program_header));

	// second segment (text)
	phdr.type = 1; // loadable
	phdr.flags = 0b101; // read + execute
	phdr.offset = 0x1000;
	phdr.vaddr = vaddr + 0x1000;
	phdr.paddr = phdr.vaddr;
	phdr.filesz = output_buffer.size() - data_size;
	phdr.memsz = phdr.filesz;
	phdr.align = 0x1000;
	f.write((const char *)&phdr, sizeof(program_header));

	// third segment (rodata)
	if (data_size) {
		phdr.type = 1; // loadable
		phdr.flags = 0b100; // read
		phdr.offset = 0x2000;
		phdr.vaddr = vaddr + 0x2000;
		phdr.paddr = phdr.vaddr;
		phdr.filesz = data_size;
		phdr.memsz = phdr.filesz;
		phdr.align = 0x1000;
		f.write((const char *)&phdr, sizeof(program_header));
	}

	// fourth segment (bss)
	if (bss_size) {
		phdr.type = 1; // loadable
		phdr.flags = 0b110; // read + write
		phdr.offset = 0x2000 + ((!!data_size) << 12);
		phdr.vaddr = vaddr + phdr.offset;
		phdr.paddr = phdr.vaddr;
		phdr.filesz = 0;
		phdr.memsz = bss_size;
		phdr.align = 0x1000;
		f.write((const char *)&phdr, sizeof(program_header));
	}

	// section headers
	// first section (null)
	section_header shdr;
	bzero(&shdr, sizeof(section_header));
	f.write((const char *)&shdr, sizeof(section_header));

	// second section (text)
	shdr.name = 1;
	shdr.type = 1; // progbits
	shdr.flags = 0x02 | 0x04; // alloc, execinstr
	shdr.addr = vaddr + 0x1000;
	shdr.offset = 0x1000;
	shdr.size = output_buffer.size() - data_size;
	shdr.link = 0;
	shdr.info = 0;
	shdr.addralign = 16;
	shdr.entsize = 0;
	f.write((const char *)&shdr, sizeof(section_header));

	// third section (rodata)
	if (data_size) {
		shdr.name = 7;
		shdr.type = 1; // progbits
		shdr.flags = 0x02; // alloc
		shdr.addr = vaddr + 0x2000;
		shdr.offset = 0x2000;
		shdr.size = data_size;
		shdr.link = 0;
		shdr.info = 0;
		shdr.addralign = 4;
		shdr.entsize = 0;
		f.write((const char *)&shdr, sizeof(section_header));
	}

	// fourth section (bss)
	if (bss_size) {
		shdr.name = 15;
		shdr.type = 8; // nobits
		shdr.flags = 0x03; // alloc, write
		shdr.addr = vaddr + 0x2000 + ((!!data_size) << 12);
		shdr.offset = 0x2000 + ((!!data_size) << 12);
		shdr.size = bss_size;
		shdr.link = 0;
		shdr.info = 0;
		shdr.addralign = 4;
		shdr.entsize = 0;
		f.write((const char *)&shdr, sizeof(section_header));
	}

	// fifth section (shstrtab)
	shdr.name = 20;
	shdr.type = 3; // strtab
	shdr.flags = 0;
	shdr.addr = 0;
	shdr.offset = 0x2000 + ((data_size + 15) & ~15);
	shdr.size = 46;
	shdr.link = 0;
	shdr.info = 0;
	shdr.addralign = 1;
	shdr.entsize = 0;
	f.write((const char *)&shdr, sizeof(section_header));

	// sixth section (strtab)
	shdr.name = 30;
	shdr.type = 3; // strtab
	shdr.flags = 0;
	shdr.addr = 0;
	shdr.offset = 0x2000 + ((data_size + 15) & ~15) + 46;
	shdr.size = 8;
	shdr.link = 0;
	shdr.info = 0;
	shdr.addralign = 1;
	shdr.entsize = 0;
	f.write((const char *)&shdr, sizeof(section_header));

	// seventh section (symtab)
	shdr.name = 38;
	shdr.type = 2; // symtab
	shdr.flags = 0;
	shdr.addr = 0;
	shdr.offset = 0x2000 + ((data_size + 15) & ~15) + 46 + 8;
	shdr.size = sizeof(symbol) * 2;
	shdr.link = 3 + !!data_size + !!bss_size; // strtab index
	shdr.info = 2; // last symbol index + 1
	shdr.addralign = 8;
	shdr.entsize = sizeof(symbol);
	f.write((const char *)&shdr, sizeof(section_header));

	// pad with zeros until 0x1000
	f.seekp(0x1000);

	// write text
	f.write((const char *)output_buffer.data() + data_size, output_buffer.size() - data_size);

	// pad with zeros until 0x2000
	f.seekp(0x2000);

	// write rodata
	if (data_size)
		f.write((const char *)output_buffer.data(), data_size);

	// pad with zeros until multiple of 16
	f.seekp((f.tellp() + (std::streamoff)15) & ~15);

	// write shstrtab
	f.write("\0.text\0.rodata\0.bss\0.shstrtab\0.strtab\0.symtab\0", 46);

	// write strtab
	f.write("\0_start\0", 8);

	// write symtab
	symbol sym;
	bzero(&sym, sizeof(symbol));
	f.write((const char *)&sym, sizeof(symbol));

	sym.name = 1;
	sym.info = 0x12; // global, function
	sym.other = 0;
	sym.shndx = 1; // text
	sym.value = vaddr + 0x1000;
	sym.size = output_buffer.size() - data_size;
	f.write((const char *)&sym, sizeof(symbol));

	std::cout << "Wrote " << f.tellp() << " bytes" << std::endl;
	f.close();
}
