//=--------------------------------------------------------------------------=//
// Since cubin has so much non-standard ELF convension. We need to replace 
// existing ELFWriter.
// Ref: ELFObjectWriter.cpp
// Note: 64-bit only
//=--------------------------------------------------------------------------=//
#include "GASSAsmBackend.h" // for createCubinObjectWriter(...)
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCFragment.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCSection.h"
#include "llvm/MC/MCSectionELF.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/MC/MCSymbolELF.h"
#include "llvm/MC/MCValue.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/StringTableBuilder.h"
#include "llvm/Support/Alignment.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/EndianStream.h"
#include <memory>
using namespace llvm;

#undef  DEBUG_TYPE
#define DEBUG_TYPE "reloc-info"

namespace {

using SectionIndexMapTy = DenseMap<const MCSectionELF *, uint32_t>;
using SymbolIndexMapTy = DenseMap<const MCSymbolELF *, uint32_t>;
using SectionOffsetsTy = DenseMap<const MCSectionELF *, 
                                  std::pair<uint64_t, uint64_t>>;


class CubinObjectWriter : public MCObjectWriter {
  /// The target specific ELF writer instance.
  std::unique_ptr<MCELFObjectTargetWriter> TargetObjectWriter;
  raw_pwrite_stream &OS;
  support::endian::Writer W;
  bool IsLittleEndian;

  unsigned StringTableIndex;
  unsigned SymbolTableIndex;

  MCSectionELF * SymbolTableSection;

  SectionIndexMapTy SectionIndexMap;
  SymbolIndexMapTy SymbolIndexMap;
  StringMap<uint32_t> SectionIndexStringMap;
  StringMap<uint32_t> SymbolIndexStringMap;
  SectionOffsetsTy SectionOffsets;
  StringMap<uint64_t> FunctionSizeStringMap;


  // Sections in the order they are to be output in the section table.
  std::vector<const MCSectionELF *> SectionTable;
  unsigned addToSectionTable(const MCSectionELF *Sec);

  // Symbols in the order they are to be output in the symbol table.
  std::vector<const MCSymbolELF *> SymbolTable;
  unsigned addToSymbolTable(const MCSymbolELF *Sym);

  StringTableBuilder StrTabBuilder{StringTableBuilder::ELF};

  /// For Program headers
  uint64_t PHdrOffset;
  uint64_t ProgbitsOffset;
  uint64_t ProgramSize;

  // symtab's sh_info
  uint64_t LastLocalSymbolIndex;

public:
  CubinObjectWriter(std::unique_ptr<MCELFObjectTargetWriter> MOTW,
                    raw_pwrite_stream &OS, bool IsLittleEndian)
      : TargetObjectWriter(std::move(MOTW)), OS(OS),
        IsLittleEndian(IsLittleEndian),
        W(OS, IsLittleEndian ? support::little : support::big) {}

  /// @return bytes written
  uint64_t writeObject(MCAssembler &Asm, const MCAsmLayout &Layout) override;

  void executePostLayoutBinding(MCAssembler &Asm,
                                const MCAsmLayout &Layout) override {}
  void recordRelocation(MCAssembler &Asm, const MCAsmLayout &Layout,
                        const MCFragment *Fragment, const MCFixup &Fixup,
                        MCValue Target, uint64_t &FixedValue) override {}

private:
  void precomputeOffsets(MCAssembler &Asm);
  void writeHeader(const MCAssembler &Asm);
  void computeSymbolTable(MCAssembler &Asm);
  void writeSectionHeader();
  void writeSymbol(unsigned StringIndex, const MCSymbol *Symbol);
  void writeSymbolEntry(uint32_t StringIndex, uint8_t Info, uint8_t Other, 
                        uint16_t SecIndex, uint64_t Value, uint64_t Size);
  void writeSectionData(MCSectionELF &Section);
  void writeSectionHeaderEntry(uint32_t StringIndex, uint32_t Type,
                               uint64_t Flags, uint64_t Addr, uint64_t Offset,
                               uint64_t Size, uint32_t Link, uint32_t Info,
                               uint64_t AddrAlign, uint64_t EntrySize);
  void writeProgramHeader();
  void align(unsigned Alignment);
};
} // end anonymous namespace

uint64_t CubinObjectWriter::writeObject(MCAssembler &Asm, 
                                        const MCAsmLayout &Layout) {
  uint64_t StartOffset = W.OS.tell();

  MCContext &Ctx = Asm.getContext();

  // a. create string table
  MCSectionELF *StrtabSection = 
      Ctx.getELFSection(".strtab", ELF::SHT_STRTAB, 0);
  StringTableIndex = addToSectionTable(StrtabSection);
  
  // b. create symtab
  unsigned EntrySize = ELF::SYMENTRY_SIZE64;
  SymbolTableSection = 
      Ctx.getELFSection(".symtab", ELF::SHT_SYMTAB, 0, EntrySize, "");
  SymbolTableSection->setAlignment(Align(8));
  SymbolTableIndex = addToSectionTable(SymbolTableSection);


  // 0. Precompute offsets, indices, etc.
  //   create symbol table
  //   compute index & offsets
  precomputeOffsets(Asm);

  // 1. Write out the ELF header ...
  writeHeader(Asm);

  // ... write StringTable
  {
    uint64_t SecStart = W.OS.tell();
    StrTabBuilder.write(W.OS);
    uint64_t SecEnd = W.OS.tell();
    SectionOffsets[StrtabSection] = std::make_pair(SecStart, SecEnd);
  }

  // 2. Compute Symbol Table (Cubin needs symbol table index)
  computeSymbolTable(Asm);

  // 3. Write Sections
  bool ProgbitsVisited = false;
  for (const MCSectionELF *Sec : SectionTable) {
    if (Sec == StrtabSection || Sec == SymbolTableSection)
      continue; // already visited

    align(Sec->getAlignment());

    uint64_t SecStart = W.OS.tell();
    // Record ProgbitsOffset
    if (Sec->getName().startswith(".nv.constant") && !ProgbitsVisited) {
      ProgbitsOffset = SecStart;
      ProgbitsVisited = true;
    }
    Asm.writeSectionData(W.OS, Sec, Layout);
    uint64_t SecEnd = W.OS.tell();
    SectionOffsets[Sec] = std::make_pair(SecStart, SecEnd);
  }

  const uint64_t SectionHeaderOffset = W.OS.tell();

  // 4. Section header table
  writeSectionHeader();

  // 5. Program header table
  PHdrOffset = W.OS.tell();
  ProgramSize = SectionHeaderOffset - ProgbitsOffset;
  writeProgramHeader();

  // update ELF header
  auto &Stream = static_cast<raw_pwrite_stream &>(W.OS);
  // Section header offset
  uint64_t Val = support::endian::byte_swap<uint64_t>(SectionHeaderOffset, 
                                                      W.Endian);
  Stream.pwrite(reinterpret_cast<char *>(&Val), sizeof(Val),
                offsetof(ELF::Elf64_Ehdr, e_shoff));
  // # sections
  uint32_t NumSectionsOffset = offsetof(ELF::Elf64_Ehdr, e_shnum);
  uint16_t NumSections = 
      support::endian::byte_swap<uint16_t>(SectionTable.size() + 1, W.Endian);
  Stream.pwrite(reinterpret_cast<char *>(&NumSections), sizeof(NumSections),
                NumSectionsOffset);
  // Program header offset
  Val = support::endian::byte_swap<uint64_t>(PHdrOffset, W.Endian);
  Stream.pwrite(reinterpret_cast<char *>(&Val), sizeof(Val),
                offsetof(ELF::Elf64_Ehdr, e_phoff));

  return W.OS.tell() - StartOffset;
}

unsigned CubinObjectWriter::addToSectionTable(const MCSectionELF *Sec) {
  // Section index starts with 1
  SectionTable.push_back(Sec);
  StrTabBuilder.add(Sec->getName());
  SectionIndexMap[Sec] = SectionTable.size();
  SectionIndexStringMap[Sec->getName()] = SectionTable.size();
  return SectionTable.size();
}

unsigned CubinObjectWriter::addToSymbolTable(const MCSymbolELF *Sym) {
  SymbolTable.push_back(Sym);
  unsigned Index = SymbolTable.size();
  SymbolIndexMap[Sym] = Index;
  SymbolIndexStringMap[Sym->getName()] = Index;
  Sym->setIndex(Index);
  StrTabBuilder.add(Sym->getName());
  return Index;
}

/// @return size of current section in bytes
static uint64_t getSectionSize(MCSectionELF *Section) {
  uint64_t Size = 0;
  for (const MCFragment &F : *Section) {
    if (F.getKind() == MCFragment::FT_Data && F.hasInstructions())
      Size += cast<MCDataFragment>(F).getContents().size();
  }
  return Size;
}

void CubinObjectWriter::precomputeOffsets(MCAssembler &Asm) {
  // We need to know index before writting Sections & Symbols
  MCContext &Ctx = Asm.getContext();

  // Order sections
  // .nv.info
  // .nv.info.{name}
  // .nv.constant0.{name}
  // .text.{name}
  // Also record section size
  std::vector<MCSectionELF *> OrderedSections;
  {
    MCSectionELF *NvInfoSec = nullptr;
    std::vector<MCSectionELF *> Others;
    std::vector<MCSectionELF *> NvInfos;
    std::vector<MCSectionELF *> Constant0s;
    std::vector<MCSectionELF *> Texts;

    for (MCSection &Sec : Asm) {
      MCSectionELF &Section = static_cast<MCSectionELF &>(Sec);
      if (Section.getName() == ".nv.info")
        NvInfoSec = &Section;
      else if (Section.getName().startswith(".nv.info."))
        NvInfos.push_back(&Section);
      else if (Section.getName().startswith(".nv.constant0."))
        Constant0s.push_back(&Section);
      else if (Section.getName().startswith(".text.")) {
        Texts.push_back(&Section);
        FunctionSizeStringMap[Section.getName().drop_front(6)] = 
          getSectionSize(&Section);
      } else if (Section.getName() == ".text")
        continue;
      else
        Others.push_back(&Section);
    }

    OrderedSections.insert(OrderedSections.end(), Others.begin(), Others.end());
    OrderedSections.push_back(NvInfoSec);
    OrderedSections.insert(OrderedSections.end(), NvInfos.begin(), 
                                                  NvInfos.end());
    OrderedSections.insert(OrderedSections.end(), Constant0s.begin(), 
                                                  Constant0s.end());
    OrderedSections.insert(OrderedSections.end(), Texts.begin(), Texts.end());
  }

  // Add sections to SectionTable
  for (MCSectionELF *Sec : OrderedSections)
    addToSectionTable(Sec);

  // Reorder symbols
  // .nv.constant0.func0
  // .text.func0
  // .nv.constant0.func1
  // .text.func1
  // func0
  // func1
  std::vector<const MCSymbolELF *> OrderedSymbols;
  {
    std::vector<const MCSymbolELF *> Constant0Symbols;
    std::vector<const MCSymbolELF *> TextSymbols;
    std::vector<const MCSymbolELF *> FuncSymbols;

    for (const MCSymbol &Sym : Asm.symbols()) {
      const MCSymbolELF &Symbol = static_cast<const MCSymbolELF &>(Sym);
      if (Symbol.getName().startswith(".nv.constant0."))
        Constant0Symbols.push_back(&Symbol);
      else if (Symbol.getName().startswith(".text."))
        TextSymbols.push_back(&Symbol);
      else if (!Symbol.getName().startswith("."))
        FuncSymbols.push_back(&Symbol);
    }

    assert(Constant0Symbols.size() == TextSymbols.size() &&
           TextSymbols.size() == FuncSymbols.size());
    
    for (int i = 0; i < Constant0Symbols.size(); ++i) {
      OrderedSymbols.push_back(Constant0Symbols[i]);
      OrderedSymbols.push_back(TextSymbols[i]);
    }
    LastLocalSymbolIndex = OrderedSymbols.size();
    OrderedSymbols.insert(OrderedSymbols.end(), FuncSymbols.begin(),
                                                FuncSymbols.end());
  }

  // Add Symbols to SymbolTable
  for (const MCSymbolELF *Sym : OrderedSymbols) {
    addToSymbolTable(Sym);
  }

  StrTabBuilder.finalize();
}

void CubinObjectWriter::writeHeader(const MCAssembler &Asm) {
  // Note: 64-bit only
  // "\x7fELF" + 64-bit + Little Endian + ...
  W.OS << ELF::ElfMagic;
  W.OS << char(ELF::ELFCLASS64);
  W.OS << char(ELF::ELFDATA2LSB); // Little endian
  W.OS << char(ELF::EV_CURRENT); // version 1
  W.OS << char(0x33); // OSABI
  W.OS << char(0x7); // ABI Version
  W.OS.write_zeros(ELF::EI_NIDENT - ELF::EI_PAD); // zero padding
  
  // Type
  W.write<uint16_t>(ELF::ET_EXEC); // 
  W.write<uint16_t>(ELF::EM_CUDA);
  W.write<uint32_t>(111); // CUDA version
  W.write<uint64_t>(0); // entry
  // TODO: update this later
  W.write<uint64_t>(0); // e_phoff
  // e_shoff will be update later
  W.write<uint64_t>(0); // e_shoff = sec hdr table off in bytes

  W.write<uint32_t>(Asm.getELFHeaderEFlags()); // cuda version

  W.write<uint16_t>(sizeof(ELF::Elf64_Ehdr)); // ehsize
  W.write<uint16_t>(56); // phentsize
  W.write<uint16_t>(3);  // phnum - cubin always has 3 program headers

  W.write<uint16_t>(sizeof(ELF::Elf64_Shdr)); // shentsize
  // # of section headers will be updated later
  W.write<uint16_t>(0); // # of section header ents

  W.write<uint16_t>(StringTableIndex);
}

void CubinObjectWriter::computeSymbolTable(MCAssembler &Asm) {
  MCContext &Ctx = Asm.getContext();

  unsigned Index = 1;
  uint64_t SecStart = W.OS.tell();

  // The first entry is the undefined symbol entry
  writeSymbolEntry(0, 0, 0, 0, 0, 0);

  for (const MCSymbolELF *Symbol : SymbolTable) {
    writeSymbol(StrTabBuilder.getOffset(Symbol->getName()), Symbol);
  }
  uint64_t SecEnd = W.OS.tell();
  SectionOffsets[SymbolTableSection] = std::make_pair(SecStart, SecEnd);
}

void CubinObjectWriter::writeSectionHeader() {
  // Null section first
  writeSectionHeaderEntry(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

  for (const MCSectionELF *Section : SectionTable) {
    const std::pair<uint64_t, uint64_t> &Offsets = 
        SectionOffsets.lookup(Section);
    uint64_t Offset = Offsets.first;
    uint64_t Size = Offsets.second - Offsets.first;
    StringRef Name = Section->getName();

    uint64_t Link = 0;
    uint64_t Info = 0;

    switch (Section->getType()) {
    default: break;
    case ELF::SHT_SYMTAB:
      Link = StringTableIndex;
      Info = LastLocalSymbolIndex + 1;
      break;
    }

    // cubin-specific
    if (Name.startswith(".nv.info.")) {
      Link = SymbolTableIndex;
      uint32_t FuncSectionIndex = 
          SectionIndexStringMap.lookup((".text." + Name.drop_front(9)).str());
      Info = FuncSectionIndex;
    } else if (Name.startswith(".nv.constant0.")) {
      uint32_t FuncSectionIndex = 
          SectionIndexStringMap.lookup((".text." + Name.drop_front(14)).str());
      Info = FuncSectionIndex;
    } else if (Name.startswith(".text.")) {
      Link = SymbolTableIndex;
      uint8_t NumRegisters = 8; // TODO: update this
      uint32_t FuncSymbolIndex = 
          SymbolIndexStringMap.lookup(Name.drop_front(6));
      Info = (NumRegisters << 24) | FuncSymbolIndex;
    }

    writeSectionHeaderEntry(StrTabBuilder.getOffset(Section->getName()),
                            Section->getType(),
                            Section->getFlags(),
                            /*Addr*/0,
                            Offset, Size, Link, Info, 
                            Section->getAlignment(),
                            Section->getEntrySize());

  }
}

void CubinObjectWriter::writeSymbol(unsigned StringIndex, 
                                    const MCSymbol *Sym) {
  // Elf64_Word st_name;		/* Symbol name, index in string tbl */
  // unsigned char	st_info;	/* Type and binding attributes */
  // unsigned char	st_other;	/* No defined meaning, 0 */
  // Elf64_Half st_shndx;		/* Associated section index */
  // Elf64_Addr st_value;		/* Value of the symbol */
  // Elf64_Xword st_size;		/* Associated symbol size */
  const MCSymbolELF * Symbol = static_cast<const MCSymbolELF *>(Sym);

  // for Info
  uint8_t Binding = Symbol->getBinding();
  uint8_t Type = Symbol->getType();
  if (!Sym->getName().startswith(".")) {
    Type = ELF::STT_FUNC;
  }
  uint8_t Info = (Binding << 4) | Type;

  uint8_t Visibility = Symbol->getVisibility();
  uint8_t Other = Symbol->getOther() | Visibility;
  const MCSectionELF &Section =
      static_cast<const MCSectionELF &>(Symbol->getSection());
  uint16_t SectionIndex = SectionIndexMap.lookup(&Section);
  uint64_t Value = 0; // Always 0
  uint64_t Size = 0; // size of kernel = size of .text. section
  if (Type == ELF::STT_FUNC)
    Size = FunctionSizeStringMap.lookup(Sym->getName());


  writeSymbolEntry(StringIndex, Info, Other, SectionIndex, Value, Size);
}

void CubinObjectWriter::writeSymbolEntry(uint32_t StringIndex, uint8_t Info, 
                                         uint8_t Other, uint16_t SecIndex,
                                         uint64_t Value, uint64_t Size) {
  W.write(StringIndex);
  W.write(Info);
  W.write(Other);
  W.write(SecIndex);
  W.write(Value);
  W.write(Size);
}

void CubinObjectWriter::writeSectionHeaderEntry(uint32_t StringIndex, 
                                                uint32_t Type, 
                                                uint64_t Flags,
                                                uint64_t Addr,
                                                uint64_t Offset,
                                                uint64_t Size,
                                                uint32_t Link,
                                                uint32_t Info,
                                                uint64_t AddrAlign,
                                                uint64_t EntrySize) {
  // struct Elf64_Shdr {
  //   Elf64_Word sh_name;
  //   Elf64_Word sh_type;
  //   Elf64_Xword sh_flags;
  //   Elf64_Addr sh_addr;
  //   Elf64_Off sh_offset;
  //   Elf64_Xword sh_size;
  //   Elf64_Word sh_link;
  //   Elf64_Word sh_info;
  //   Elf64_Xword sh_addralign;
  //   Elf64_Xword sh_entsize;
  // };
  W.write(StringIndex);
  W.write(Type);
  W.write(Flags);
  W.write(Addr);
  W.write(Offset);
  W.write(Size);
  W.write(Link);
  W.write(Info);
  W.write(AddrAlign);
  W.write(EntrySize);
}

void CubinObjectWriter::writeProgramHeader() {
  // 3 programs
  ELF::Elf64_Phdr PHdr{
    .p_type = ELF::PT_PHDR, 
    .p_flags = 5,
    .p_offset = PHdrOffset, // Start of Program Header
    .p_vaddr = 0, .p_paddr = 0,
    .p_filesz = 0xa8,
    .p_memsz = 0xa8,
    .p_align = 8
  };
  ELF::Elf64_Phdr PProgbits{
    .p_type = ELF::PT_LOAD, 
    .p_flags = 5,
    .p_offset = ProgbitsOffset,
    .p_vaddr = 0, .p_paddr = 0, 
    .p_filesz = ProgramSize, 
    .p_memsz = ProgramSize, 
    .p_align = 8
  };
  ELF::Elf64_Phdr PNobits{
    .p_type = ELF::PT_LOAD, 
    .p_flags = 6,
    .p_offset = 0,
    .p_vaddr = 0, .p_paddr = 0, .p_filesz = 0, .p_memsz = 0, .p_align = 8
  };

  char Buf[sizeof(ELF::Elf64_Phdr)];
  memcpy(Buf, &PHdr, sizeof(PHdr));
  for (int i = 0; i < sizeof(ELF::Elf64_Phdr); ++i)
    W.OS << Buf[i];
  memcpy(Buf, &PProgbits, sizeof(PProgbits));
  for (int i = 0; i < sizeof(ELF::Elf64_Phdr); ++i)
    W.OS << Buf[i];
  memcpy(Buf, &PNobits, sizeof(PNobits));
  for (int i = 0; i < sizeof(ELF::Elf64_Phdr); ++i)
    W.OS << Buf[i];
}

void CubinObjectWriter::align(unsigned Alignment) {
  uint64_t Padding = offsetToAlignment(W.OS.tell(), Align(Alignment));
  W.OS.write_zeros(Padding);
}

std::unique_ptr<MCObjectWriter>
llvm::createCubinObjectWriter(std::unique_ptr<MCELFObjectTargetWriter> MOTW,
                              raw_pwrite_stream &OS, bool IsLittleEndian) {
  return std::make_unique<CubinObjectWriter>(std::move(MOTW), OS, 
                                             IsLittleEndian);
}