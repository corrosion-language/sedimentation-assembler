#include "coff.hpp"

void generate_coff(std::ofstream &f, uint64_t bss_size) {
	uint64_t strtab_size = 4;
	for (auto &s : extern_labels) {
		if (s.size() > 8)
			strtab_size += s.size() + 1;
	}
	for (auto &s : labels) {
		if (s.first.size() > 8)
			strtab_size += s.first.size() + 1;
	}

	const size_t data_size = data_buffer.size();
	const size_t rodata_size = rodata_buffer.size();

	// COFF header
	coff_header chdr;
	chdr.sections = 1 + !!data_size + !!rodata_size + !!bss_size;
	chdr.timestamp = (uint32_t)time(NULL);
	chdr.symtab_off = sizeof(chdr) + chdr.sections * sizeof(coff_section_header); // coff hdr + section headers
	chdr.num_symbols = extern_labels.size() + labels.size();
	f.write((const char *)&chdr, sizeof(coff_header));

	// section headers
	// text section
	coff_section_header shdr;
	memcpy(shdr.name, ".text\0\0", 8);
	shdr.vsize = text_buffer.size();
	shdr.size = text_buffer.size();
	shdr.offset = chdr.symtab_off + chdr.num_symbols * sizeof(coff_symbol) + strtab_size;
	shdr.reloc_off = relocations.size() ? shdr.offset + shdr.size : 0;
	shdr.num_relocs = relocations.size();
	shdr.flags = 0x60500020; // code, execute, read, align 16
	f.write((const char *)&shdr, sizeof(shdr));

	size_t next_offset = (shdr.offset + shdr.size + shdr.num_relocs * sizeof(coff_relocation));

	// data section
	if (data_size) {
		memcpy(shdr.name, ".data\0\0", 8);
		shdr.vsize = data_size;
		shdr.size = data_size;
		shdr.offset = next_offset;
		shdr.reloc_off = 0;
		shdr.num_relocs = 0;
		shdr.flags = 0xc0300040; // initialized data, read, write, align 4
		f.write((const char *)&shdr, sizeof(shdr));

		next_offset = shdr.offset + shdr.size;
	}

	// rodata section
	if (rodata_size) {
		memcpy(shdr.name, ".rodata", 8);
		shdr.vsize = rodata_size;
		shdr.size = rodata_size;
		shdr.offset = next_offset;
		shdr.reloc_off = 0;
		shdr.num_relocs = 0;
		shdr.flags = 0x40300040; // initialized data, read, align 4
		f.write((const char *)&shdr, sizeof(shdr));

		next_offset = shdr.offset + shdr.size;
	}

	// bss section
	if (bss_size) {
		memcpy(shdr.name, ".bss\0\0\0", 8);
		shdr.vsize = bss_size;
		shdr.size = 0;
		shdr.offset = 0;
		shdr.reloc_off = 0;
		shdr.num_relocs = 0;
		shdr.flags = 0xc0300080; // uninitialized data, read, write, align 4
		f.write((const char *)&shdr, sizeof(shdr));
	}

	// symbol table
	coff_symbol sym;
	uint32_t i = 4;
	std::vector<std::string> ordered_syms;
	for (auto &l : labels) {
		if (l.first.size() <= 8) {
			memset(sym.name, 0, 8);
			memcpy(sym.name, l.first.data(), l.first.size());
		} else {
			memset(sym.name, 0, 4);
			memcpy(sym.name + 4, (const char *)&i, 4);
			i += l.first.size() + 1;
		}
		sym.val = l.second.second;
		if (l.second.first == TEXT)
			sym.storage_class = 2; // external
		else
			sym.storage_class = 3; // static

		if (l.second.first == TEXT)
			sym.section = 1;
		else if (l.second.first == DATA)
			sym.section = 2;
		else if (l.second.first == RODATA)
			sym.section = 2 + !!data_size;
		else
			sym.section = 2 + !!data_size + !!rodata_size;
		ordered_syms.push_back(l.first);
		f.write((const char *)&sym, sizeof(sym));
	}
	sym.storage_class = 5; // external
	for (auto &l : extern_labels) {
		if (l.size() <= 8) {
			memset(sym.name, 0, 8);
			memcpy(sym.name, l.data(), l.size());
		} else {
			memset(sym.name, 0, 4);
			memcpy(sym.name + 4, (const char *)&i, 4);
			i += l.size() + 1;
		}
		sym.val = 0;
		ordered_syms.push_back(l);
		f.write((const char *)&sym, sizeof(sym));
	}

	// string table
	f.write((const char *)&strtab_size, 4);
	for (auto &l : ordered_syms) {
		if (l.size() <= 8)
			continue;
		f.write(l.data(), l.size() + 1);
	}

	// relocation addends
	for (auto &r : relocations) {
		if (r.type == REL)
			r.addend += 4;

		if (r.size == 8) {
			text_buffer[r.offset] = r.addend;
		} else if (r.size == 16) {
			*(int16_t *)(text_buffer.data() + r.offset) = r.addend;
		} else if (r.size == 32) {
			*(int32_t *)(text_buffer.data() + r.offset) = r.addend;
		} else {
			*(int64_t *)(text_buffer.data() + r.offset) = r.addend;
		}
	}

	// text
	f.write((const char *)text_buffer.data(), text_buffer.size());

	// relocation table
	coff_relocation rel;
	for (auto &r : relocations) {
		rel.vaddr = r.offset;
		rel.sym = find(ordered_syms.begin(), ordered_syms.end(), r.symbol) - ordered_syms.begin();
		if (r.type == ABS) {
			std::cerr << "avertissement : les rédressages absolus peuvent être tronqués pendant le linkage" << std::endl;
			rel.type = 2;
		} else if (r.type == REL) {
			rel.type = 4;
		} else {
			std::cerr << "avertissement : impossible de créer un réadressage vers PLT dans un fichier COFF" << std::endl;
			rel.type = 0;
		}
		f.write((const char *)&rel, sizeof(rel));
	}

	// data
	f.write((const char *)data_buffer.data(), data_size);

	// rodata
	f.write((const char *)rodata_buffer.data(), rodata_size);

	f.close();
}
