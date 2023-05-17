#include "elf.hpp"

void rand_bytes(char *buf, size_t len) {
	std::ifstream f("/dev/urandom", std::ios::binary);
	f.read(buf, len);
	f.close();
}

void generate_elf(std::ofstream &f, uint64_t bss_size) {
	// structure:
	//  ELF header
	//  section headers
	//   text
	//   data
	//   bss
	//   shstrtab
	//   symtab
	//   strtab
	//   rela.text
	//  section data

	// list of symbols to include
	uint64_t strtab_size = 1;
	for (auto &s : extern_labels)
		strtab_size += s.size() + 1;
	for (auto &s : labels)
		strtab_size += s.first.size() + 1;

	const size_t data_size = data_buffer.size();

	// ELF header
	elf_header ehdr;
	memcpy(ehdr.ident, "\x7f\x45LF\x02\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00", 16);
	ehdr.type = 1; // relocatable
	ehdr.machine = 0x3e; // x86-64
	ehdr.version = 1; // current
	ehdr.entry = 0;
	ehdr.phoff = 0;
	ehdr.shoff = sizeof(elf_header);
	ehdr.flags = 0;
	ehdr.ehsize = sizeof(elf_header);
	ehdr.phentsize = 0;
	ehdr.phnum = 0;
	ehdr.shentsize = sizeof(section_header);
	ehdr.shnum = 5 + !!data_size + !!bss_size + !!relocations.size(); // 5 for null, text, symtab, strtab, shstrtab
	ehdr.shstrndx = 2 + !!data_size + !!bss_size; // first metadata section for shstrtab
	f.write((const char *)&ehdr, sizeof(elf_header));

	// section headers
	section_header shdr;
	// null section
	bzero(&shdr, sizeof(section_header));
	f.write((const char *)&shdr, sizeof(section_header));

	// text section
	shdr.name = 1;
	shdr.type = 1; // progbits
	shdr.flags = 0x2 | 0x4; // alloc, execinstr
	shdr.addr = 0;
	shdr.offset = ehdr.shoff + ehdr.shentsize * ehdr.shnum;
	shdr.size = text_buffer.size();
	shdr.link = 0;
	shdr.info = 0;
	shdr.addralign = 16;
	shdr.entsize = 0;
	f.write((const char *)&shdr, sizeof(section_header));

	size_t next_offset = (shdr.offset + shdr.size + 15) & ~15;

	// data section
	if (data_size) {
		shdr.name = 7;
		shdr.type = 1; // progbits
		shdr.flags = 0x2 | 0x1; // alloc, write
		shdr.addr = 0;
		shdr.offset = next_offset;
		shdr.size = data_size;
		shdr.link = 0;
		shdr.info = 0;
		shdr.addralign = 4;
		shdr.entsize = 0;
		f.write((const char *)&shdr, sizeof(section_header));

		next_offset = (shdr.offset + shdr.size + 3) & ~3;
	}

	// bss section
	if (bss_size) {
		shdr.name = 13;
		shdr.type = 8; // nobits
		shdr.flags = 0x2 | 0x1; // alloc, write
		shdr.addr = 0;
		shdr.offset = 0;
		shdr.size = bss_size;
		shdr.link = 0;
		shdr.info = 0;
		shdr.addralign = 1;
		shdr.entsize = 0;
		f.write((const char *)&shdr, sizeof(section_header));
	}

	// shstrtab section
	shdr.name = 18;
	shdr.type = 3; // strtab
	shdr.flags = 0;
	shdr.addr = 0;
	shdr.offset = next_offset;
	shdr.size = 55;
	shdr.link = 0;
	shdr.info = 0;
	shdr.addralign = 1;
	shdr.entsize = 0;
	f.write((const char *)&shdr, sizeof(section_header));

	// symbol table
	shdr.name = 28;
	shdr.type = 2; // symtab
	shdr.flags = 0;
	shdr.addr = 0;
	shdr.offset = shdr.offset + shdr.size;
	shdr.size = (labels.size() + extern_labels.size() + 1) * sizeof(symbol);
	shdr.link = ehdr.shnum - 1 - !!relocations.size(); // strtab
	shdr.info = shdr.size / sizeof(symbol) - global.size() - extern_labels.size(); // index of last local symbol + 1
	shdr.addralign = 8;
	shdr.entsize = sizeof(symbol);
	f.write((const char *)&shdr, sizeof(section_header));

	// strtab section
	shdr.name = 36;
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

	// relocation table
	if (relocations.size()) {
		shdr.name = 44;
		shdr.type = 4; // rela
		shdr.flags = 0;
		shdr.addr = 0;
		shdr.offset = shdr.offset + shdr.size;
		shdr.size = relocations.size() * sizeof(relocation);
		shdr.link = ehdr.shnum - 3; // symtab
		shdr.info = 1; // text section
		shdr.addralign = 8;
		shdr.entsize = sizeof(relocation);
		f.write((const char *)&shdr, sizeof(section_header));
	}

	// write text
	f.write((const char *)text_buffer.data(), text_buffer.size());

	// seek to multiple of 16
	f.seekp((f.tellp() + (std::streamoff)15) & ~15);

	// write data
	f.write((const char *)data_buffer.data(), data_size);

	// seek to multiple of 4
	f.seekp((f.tellp() + (std::streamoff)3) & ~3);

	// write shstrtab
	f.write("\0.text\0.data\0.bss\0.shstrtab\0.symtab\0.strtab\0.rela.text", 55);

	std::vector<std::string> ordered_labels;

	uint64_t i = 1;
	symbol sym;
	// null symbol
	bzero(&sym, sizeof(symbol));
	f.write((const char *)&sym, sizeof(symbol));
	sym.info = 0;
	// write symtab
	for (const auto &l : labels) {
		if (global.count(l.first))
			continue;
		sym.name = i;
		i += l.first.size() + 1;
		sym.other = 0;
		if (l.second.first == TEXT)
			sym.shndx = 1; // text
		else if (l.second.first == DATA)
			sym.shndx = 2; // data
		else
			sym.shndx = 2 + !!data_size; // bss
		sym.value = l.second.second;
		sym.size = 0;
		f.write((const char *)&sym, sizeof(symbol));
		ordered_labels.push_back(l.first);
	}
	sym.info = 0x10;
	sym.shndx = 0;
	for (const auto &s : extern_labels) {
		sym.name = i;
		i += s.size() + 1;
		f.write((const char *)&sym, sizeof(symbol));
		ordered_labels.push_back(s);
	}
	for (const auto &l : labels) {
		if (!global.count(l.first))
			continue;
		sym.name = i;
		i += l.first.size() + 1;
		sym.other = 0;
		if (l.second.first == TEXT)
			sym.shndx = 1; // text
		else if (l.second.first == DATA)
			sym.shndx = 2; // data
		else
			sym.shndx = 2 + !!data_size; // bss
		sym.value = l.second.second;
		sym.size = 0;
		f.write((const char *)&sym, sizeof(symbol));
		ordered_labels.push_back(l.first);
	}

	// write strtab
	f.seekp(f.tellp() + (std::streamoff)1);
	for (auto &l : ordered_labels)
		if (l.size())
			f.write(l.c_str(), l.size() + 1);

	relocation reloc;
	// write rela.text
	for (auto &r : relocations) {
		reloc.offset = r.offset;
		uint64_t index = std::find(ordered_labels.begin(), ordered_labels.end(), r.symbol) - ordered_labels.begin();
		reloc.info = (index + 1) << 32;
		if (r.type == ABS) {
			if (r.size == 64)
				reloc.info |= 1;
			else if (r.size == 32)
				reloc.info |= 11;
		} else if (r.type == REL)
			reloc.info |= 2;
		else
			reloc.info |= 4;
		reloc.addend = r.addend;
		f.write((const char *)&reloc, sizeof(relocation));
	}

	// close file
	f.close();
}
