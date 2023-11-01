#include "elf.hpp"

#include <elf.h>
#include <fstream>

void ELF_Write(std::vector<Section> sections, std::vector<Symbol> symbols, std::string filename) {
	// Open file
	std::ofstream f(filename, std::ios::binary);
	uint8_t zero[64] = {0};

	// Write the ELF header
	Elf64_Ehdr ehdr;
	ehdr.e_ident[EI_MAG0] = ELFMAG0; // 0x7f
	ehdr.e_ident[EI_MAG1] = ELFMAG1; // 'E'
	ehdr.e_ident[EI_MAG2] = ELFMAG2; // 'L'
	ehdr.e_ident[EI_MAG3] = ELFMAG3; // 'F'
	ehdr.e_ident[EI_CLASS] = ELFCLASS64; // 64-bit
	ehdr.e_ident[EI_DATA] = ELFDATA2LSB; // little-endian
	ehdr.e_ident[EI_VERSION] = EV_CURRENT; // current version
	ehdr.e_ident[EI_OSABI] = ELFOSABI_SYSV; // UNIX System V ABI
	ehdr.e_ident[EI_ABIVERSION] = 0; // ABI version
	ehdr.e_type = ET_REL; // relocatable
	ehdr.e_machine = EM_X86_64; // x86-64
	ehdr.e_version = EV_CURRENT; // current version
	ehdr.e_phoff = 0; // no program header table
	ehdr.e_shoff = sizeof(Elf64_Ehdr); // section header table offset
	ehdr.e_flags = 0; // no flags
	ehdr.e_ehsize = sizeof(Elf64_Ehdr); // ELF header size
	ehdr.e_phentsize = 0; // no program header table
	ehdr.e_phnum = 0; // no program header table
	ehdr.e_shentsize = sizeof(Elf64_Shdr); // section header size
	ehdr.e_shnum = sections.size() + 4; // null + number of sections + symtab + strtab + shstrtab
	ehdr.e_shstrndx = sections.size() + 3; // index of section name string table (last section)
	f.write((char *)&ehdr, sizeof(Elf64_Ehdr));

	// Write the null section header
	f.write((char *)zero, sizeof(Elf64_Shdr));

	// Offset for section data
	Elf64_Off dataoff = 0;

	std::string shstrtab;
	shstrtab.push_back(0); // null section name
	// Write the section header table
	for (auto section : sections) {
		/// TODO: Take into account special sections (e.g. .bss)
		Elf64_Shdr shdr;
		shdr.sh_name = shstrtab.size(); // name offset in section name string table
		shstrtab += section.name + '\0'; // add section name to section name string table
		shdr.sh_type = SHT_PROGBITS; // section type
		shdr.sh_flags = 0; // no flags
		shdr.sh_addr = 0; // no address
		shdr.sh_offset = dataoff; // section offset
		shdr.sh_size = section.data.size(); // section size
		dataoff += (section.data.size() + 0xfff) & ~0xfff;
		shdr.sh_link = 0; // no link
		shdr.sh_info = 0; // no info
		shdr.sh_addralign = 1; // no alignment
		shdr.sh_entsize = 0; // no entry size
		f.write((char *)&shdr, sizeof(Elf64_Shdr));
	}

	std::string strtab;
	strtab.push_back(0); // null symbol name
	std::vector<uint8_t> symtab;
	// Write the symbol table
	for (auto sym : symbols) {
		Elf64_Sym esym;
		esym.st_name = strtab.size(); // name offset in string table
		strtab += sym.name + '\0'; // add name to string table
		esym.st_info =
			ELF64_ST_INFO(sym.global ? STB_GLOBAL : STB_LOCAL, sym.type); // symbol binding and type
		esym.st_other = 0; // no other
		esym.st_shndx = 1; // section index
		esym.st_value = sym.value; // symbol value
		esym.st_size = 0; // no size
		symtab.insert(symtab.end(), (uint8_t *)&esym, (uint8_t *)&esym + sizeof(Elf64_Sym));
	}

	// Offset into file for writing metadata
	Elf64_Off metaoff = (Elf64_Off)f.tellp() + 3 * sizeof(Elf64_Shdr);

	Elf64_Shdr shdr;
	// Write symtab header
	shdr.sh_name = shstrtab.size(); // name offset in section name string table
	shstrtab += ".symtab"; // add section name to section name string table
	shstrtab.push_back(0);
	shdr.sh_type = SHT_SYMTAB; // section type
	shdr.sh_flags = 0; // no flags
	shdr.sh_addr = 0; // no address
	shdr.sh_offset = metaoff; // no offset
	shdr.sh_size = symtab.size(); // section size
	metaoff += symtab.size();
	shdr.sh_link = sections.size() + 2; // link to string table
	shdr.sh_info = symbols.size(); // number of symbols
	shdr.sh_addralign = 8; // alignment
	shdr.sh_entsize = sizeof(Elf64_Sym); // entry size
	f.write((char *)&shdr, sizeof(Elf64_Shdr));

	// Write strtab header
	shdr.sh_name = shstrtab.size(); // name offset in section name string table
	shstrtab += ".strtab"; // add section name to section name string table
	shstrtab.push_back(0);
	shdr.sh_type = SHT_STRTAB; // section type
	shdr.sh_flags = 0; // no flags
	shdr.sh_addr = 0; // no address
	shdr.sh_offset = metaoff; // no offset
	shdr.sh_size = strtab.size(); // section size
	metaoff += strtab.size();
	shdr.sh_link = 0; // no link
	shdr.sh_info = 0; // no info
	shdr.sh_addralign = 1; // no alignment
	shdr.sh_entsize = 0; // no entry size
	f.write((char *)&shdr, sizeof(Elf64_Shdr));

	// Write shstrtab header
	shdr.sh_name = shstrtab.size(); // name offset in section name string table
	shstrtab += ".shstrtab"; // add section name to section name string table
	shstrtab.push_back(0);
	shdr.sh_type = SHT_STRTAB; // section type
	shdr.sh_flags = 0; // no flags
	shdr.sh_addr = 0; // no address
	shdr.sh_offset = metaoff; // no offset
	shdr.sh_size = shstrtab.size(); // section size
	metaoff += shstrtab.size();
	shdr.sh_link = 0; // no link
	shdr.sh_info = 0; // no info
	shdr.sh_addralign = 1; // no alignment
	shdr.sh_entsize = 0; // no entry size
	f.write((char *)&shdr, sizeof(Elf64_Shdr));

	f.write((const char *)symtab.data(), symtab.size());
	f.write(strtab.data(), strtab.size());
	f.write(shstrtab.data(), shstrtab.size());

	// Write the section data
	for (auto section : sections) {
		f.seekp((f.tellp() + (std::streampos)0x1000) & ~0xfff, std::ios::beg);
		f.write((char *)section.data.data(), section.data.size());
	}
	f.seekp((f.tellp() + (std::streampos)0x1000) & ~0xfff, std::ios::beg);
}
