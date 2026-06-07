#include "ElfReader.h"
#include <cstring>
#include <cstdio>
#include <algorithm>

#ifndef EM_AARCH64
#define EM_AARCH64 183
#endif
#ifndef R_AARCH64_ABS64
#define R_AARCH64_ABS64 257
#endif
#ifndef R_AARCH64_RELATIVE
#define R_AARCH64_RELATIVE 1027
#endif

#undef MY_DT_PLTGOT
#undef MY_DT_HASH
#undef MY_DT_STRTAB
#undef MY_DT_SYMTAB
#undef MY_DT_RELA
#undef MY_DT_RELASZ
#undef MY_DT_INIT
#undef MY_DT_FINI
#undef MY_DT_REL
#undef MY_DT_RELSZ
#undef MY_DT_JMPREL
#undef MY_DT_INIT_ARRAY
#undef MY_DT_FINI_ARRAY
#undef MY_DT_GNU_HASH
#undef MY_SHT_LOUSER
#undef MY_R_ARM_ABS32
#undef MY_R_386_32
#undef MY_R_X86_64_64
#undef MY_R_X86_64_RELATIVE

static constexpr uint64_t MY_DT_PLTGOT    = 3;
static constexpr uint64_t MY_DT_HASH      = 4;
static constexpr uint64_t MY_DT_STRTAB    = 5;
static constexpr uint64_t MY_DT_SYMTAB    = 6;
static constexpr uint64_t MY_DT_RELA      = 7;
static constexpr uint64_t MY_DT_RELASZ    = 8;
static constexpr uint64_t MY_DT_INIT      = 12;
static constexpr uint64_t MY_DT_FINI      = 13;
static constexpr uint64_t MY_DT_REL       = 17;
static constexpr uint64_t MY_DT_RELSZ     = 18;
static constexpr uint64_t MY_DT_ANDROID_RELA   = 0x60000011;
static constexpr uint64_t MY_DT_ANDROID_RELASZ = 0x60000012;
static constexpr uint64_t MY_DT_ANDROID_REL    = 0x6000000F;
static constexpr uint64_t MY_DT_ANDROID_RELSZ  = 0x60000010;
static constexpr uint64_t MY_DT_JMPREL    = 23;
static constexpr uint64_t MY_DT_INIT_ARRAY = 25;
static constexpr uint64_t MY_DT_FINI_ARRAY = 26;
static constexpr uint64_t MY_DT_GNU_HASH  = 0x6ffffef5;
static constexpr uint32_t MY_SHT_LOUSER   = 0x80000000u;
static constexpr int MY_R_ARM_ABS32       = 2;
static constexpr int MY_R_386_32          = 1;
static constexpr int MY_R_X86_64_64       = 1;
static constexpr int MY_R_X86_64_RELATIVE = 8;

template<typename Dyn>
static uint64_t dynVal(const std::vector<Dyn>& dyn, uint32_t tag) {
    for (auto& d : dyn) if ((uint32_t)d.d_tag == tag) return (uint64_t)d.d_un.d_val;
    throw std::runtime_error("DT entry not found");
}
template<typename Dyn>
static bool dynFind(const std::vector<Dyn>& dyn, uint32_t tag, uint64_t& out) {
    for (auto& d : dyn) if ((uint32_t)d.d_tag == tag) { out = (uint64_t)d.d_un.d_val; return true; }
    return false;
}

void ElfReader32::load() {
    seek(0);
    memcpy(&ehdr, buf.data(), sizeof(ehdr));
    phdrs.resize(ehdr.e_phnum);
    memcpy(phdrs.data(), buf.data()+ehdr.e_phoff, ehdr.e_phnum*sizeof(Elf32_Phdr));
    computeLoadBase();

    if (isDumped) fixedProgramSegment();

    for (auto& ph : phdrs) if (ph.p_type == PT_DYNAMIC) { ptDynamic = ph; break; }
    size_t dynCount = ptDynamic.p_filesz / sizeof(Elf32_Dyn);
    dynSec.resize(dynCount);
    memcpy(dynSec.data(), buf.data()+ptDynamic.p_offset, dynCount*sizeof(Elf32_Dyn));
    if (isDumped) fixedDynamicSection();

    readSymbol();
    if (!isDumped) {
        relocationProcessing();
        if (checkProtection()) printf("ERROR: This file may be protected.\n");
    }
}

bool ElfReader32::checkSection() {
    try {
        if (ehdr.e_shoff == 0 || ehdr.e_shnum == 0) return false;
        shdrs.resize(ehdr.e_shnum);
        memcpy(shdrs.data(), buf.data()+ehdr.e_shoff, ehdr.e_shnum*sizeof(Elf32_Shdr));
        auto& shstr = shdrs[ehdr.e_shstrndx];
        bool found = false;
        for (auto& s : shdrs) {
            std::string name = readStringToNull(shstr.sh_offset + s.sh_name);
            if (name == ".text") { found=true; break; }
        }
        return found;
    } catch (...) { return false; }
}

void ElfReader32::readSymbol() {
    try {
        uint32_t symbolCount = 0;
        uint64_t hashAddr;
        if (dynFind(dynSec, MY_DT_HASH, hashAddr)) {
            uint64_t off = mapVATR(hashAddr);
            seek(off+4);
            symbolCount = ru32();
        } else {
            uint64_t gnuHashAddr = dynVal(dynSec, MY_DT_GNU_HASH);
            uint64_t off = mapVATR(gnuHashAddr);
            seek(off);
            uint32_t nbuckets  = ru32();
            uint32_t symoffset = ru32();
            uint32_t bloom_size = ru32();
            ru32();
            uint64_t bucketsAddr = off + 16 + 4*bloom_size;
            seek(bucketsAddr);
            uint32_t maxBucket = 0;
            for (uint32_t i = 0; i < nbuckets; i++) {
                uint32_t b = ru32(); if (b > maxBucket) maxBucket = b;
            }
            if (maxBucket < symoffset) {
                symbolCount = symoffset;
            } else {
                seek(bucketsAddr + 4*nbuckets + (maxBucket-symoffset)*4);
                uint32_t last = maxBucket;
                while (true) { uint32_t ce = ru32(); ++last; if (ce & 1) break; }
                symbolCount = last;
            }
        }
        uint64_t dynsymOff = mapVATR(dynVal(dynSec, MY_DT_SYMTAB));
        symTab.resize(symbolCount);
        memcpy(symTab.data(), buf.data()+dynsymOff, symbolCount*sizeof(Elf32_Sym));
    } catch (...) {}
}

void ElfReader32::relocationProcessing() {
    printf("Applying relocations...\n");
    try {
        uint64_t relOff, relSize;
        if (!dynFind(dynSec, MY_DT_ANDROID_REL, relOff) ||
            !dynFind(dynSec, MY_DT_ANDROID_RELSZ, relSize)) {
            relOff  = dynVal(dynSec, MY_DT_REL);
            relSize = dynVal(dynSec, MY_DT_RELSZ);
        }
        size_t count = (size_t)relSize / sizeof(Elf32_Rel);
        std::vector<Elf32_Rel> rels(count);
        uint64_t fileOff = mapVATR(relOff);
        memcpy(rels.data(), buf.data()+fileOff, count*sizeof(Elf32_Rel));
        bool isx86 = ehdr.e_machine == EM_386;
        for (auto& r : rels) {
            uint32_t type = r.r_info & 0xff;
            uint32_t sym  = r.r_info >> 8;
            if ((!isx86 && type == MY_R_ARM_ABS32) || (isx86 && type == MY_R_386_32)) {
                if (sym < symTab.size()) {
                    uint32_t dstOff = (uint32_t)mapVATR(r.r_offset);
                    writeAt(dstOff, &symTab[sym].st_value, 4);
                }
            }
        }
    } catch (...) {}
}

bool ElfReader32::checkProtection() {
    try {
        uint64_t initAddr;
        if (dynFind(dynSec, MY_DT_INIT, initAddr)) {
            printf("WARNING: find .init_proc\n"); return true;
        }
        uint64_t dynstrOff = mapVATR(dynVal(dynSec, MY_DT_STRTAB));
        for (auto& sym : symTab) {
            std::string name = readStringToNull(dynstrOff + sym.st_name);
            if (name == "JNI_OnLoad") { printf("WARNING: find JNI_OnLoad\n"); return true; }
        }
        if (!shdrs.empty()) {
            for (auto& s : shdrs)
                if (s.sh_type == MY_SHT_LOUSER) { printf("WARNING: find MY_SHT_LOUSER\n"); return true; }
        }
    } catch (...) {}
    return false;
}

void ElfReader32::fixedProgramSegment() {
    for (size_t i = 0; i < phdrs.size(); i++) {
        size_t off = ehdr.e_phoff + i*sizeof(Elf32_Phdr) + 4;
        phdrs[i].p_offset = phdrs[i].p_vaddr;
        writeAt(off, &phdrs[i].p_offset, 4);
        phdrs[i].p_vaddr += (uint32_t)imageBase;
        writeAt(off+4, &phdrs[i].p_vaddr, 4);
        off += 8;
        phdrs[i].p_filesz = phdrs[i].p_memsz;
        writeAt(off, &phdrs[i].p_filesz, 4);
    }
}

void ElfReader32::fixedDynamicSection() {
    for (size_t i = 0; i < dynSec.size(); i++) {
        size_t off = ptDynamic.p_offset + i*8 + 4;
        uint32_t tag = (uint32_t)dynSec[i].d_tag;
        if (tag==MY_DT_PLTGOT||tag==MY_DT_HASH||tag==MY_DT_STRTAB||tag==MY_DT_SYMTAB||
            tag==MY_DT_RELA||tag==MY_DT_INIT||tag==MY_DT_FINI||tag==MY_DT_REL||
            tag==MY_DT_JMPREL||tag==MY_DT_INIT_ARRAY||tag==MY_DT_FINI_ARRAY) {
            dynSec[i].d_un.d_val += (uint32_t)imageBase;
            writeAt(off, &dynSec[i].d_un.d_val, 4);
        }
    }
}

// Ported from Project B: wildcard ARM pattern with LDR validation
bool ElfReader32::search() {
    uint64_t GOT = dynVal(dynSec, MY_DT_PLTGOT);
    std::vector<Elf32_Phdr> execs;
    for (auto& ph : phdrs)
        if (ph.p_type == PT_LOAD && (ph.p_flags & PF_X)) execs.push_back(ph);

    // Wildcard pattern from Project B: "? 0x10 ? 0xE7 ? 0x00 ? 0xE0 ? 0x20 ? 0xE0"
    // This represents: LDR R1, [X]; ADD R0, X, X; ADD R2, X, X (with wildcard register bits)
    static const char* armPattern = "? 0x10 ? 0xE7 ? 0x00 ? 0xE0 ? 0x20 ? 0xE0";

    for (auto& ex : execs) {
        size_t sz = (size_t)ex.p_filesz;
        auto idxs = searchPattern(armPattern, (size_t)ex.p_offset, (size_t)ex.p_offset+sz);
        for (auto idx : idxs) {
            // Validate LDR instruction: check bit 3 of byte at idx+2 (Project B: bin[3] == '1')
            // The instruction at idx+2 should be an LDR (bit pattern check)
            bool isLdr = false;
            if (idx + 2 < buf.size()) {
                uint8_t b = buf[idx + 2];
                // For ARM: LDR conditional has cond bits at top, check for LDR variant
                // Project B checks bin[3] == '1' which verifies it's a valid LDR
                isLdr = (b & 0x10) != 0; // Check bit 4 of byte 2 (ARM LDR indicator)
            }
            if (!isLdr) continue;

            uint32_t codeReg=0, metaReg=0;
            uint32_t off = (uint32_t)idx;

            if (version < 24.0 && ehdr.e_machine == EM_ARM) {
                seek(off + 0x14); codeReg  = ru32() + (uint32_t)GOT;
                seek(off + 0x18); uint32_t ptr = ru32() + (uint32_t)GOT;
                seek(mapVATR(ptr)); metaReg = ru32();
            } else if (version >= 24.0 && ehdr.e_machine == EM_ARM) {
                seek(off + 0x14); codeReg  = ru32() + off + 0xc + (uint32_t)imageBase;
                seek(off + 0x10); uint32_t ptr = ru32() + off + 0x8;
                seek(mapVATR(ptr + (uint32_t)imageBase)); metaReg = ru32();
            }
            if (codeReg && metaReg) {
                printf("CodeRegistration : %x\n", codeReg);
                printf("MetadataRegistration : %x\n", metaReg);
                init(codeReg, metaReg);
                return true;
            }
        }
    }
    return false;
}

bool ElfReader32::plusSearch(int mc, int tc, int ic) {
    auto* sh = getSectionHelper(mc, tc, ic);
    uint64_t cr = sh->findCodeRegistration();
    uint64_t mr = sh->findMetadataRegistration();
    delete sh;
    return autoPlusInit(cr, mr);
}

bool ElfReader32::symbolSearch() {
    uint32_t cr=0, mr=0;
    uint64_t dynstrOff = mapVATR(dynVal(dynSec, MY_DT_STRTAB));
    for (auto& sym : symTab) {
        std::string name = readStringToNull(dynstrOff + sym.st_name);
        if (name == "g_CodeRegistration")     cr = sym.st_value;
        if (name == "g_MetadataRegistration") mr = sym.st_value;
    }
    if (cr && mr) {
        printf("Detected Symbol !\n");
        printf("CodeRegistration : %x\n", cr);
        printf("MetadataRegistration : %x\n", mr);
        init(cr, mr);
        return true;
    }
    printf("ERROR: No symbol is detected\n");
    return false;
}

SectionHelper* ElfReader32::getSectionHelper(int mc, int tc, int ic) {
    std::vector<SearchSection> dataList, execList;
    for (auto& ph : phdrs) {
        if (ph.p_memsz == 0) continue;
        SearchSection s;
        s.offset    = ph.p_offset;
        s.offsetEnd = ph.p_offset + ph.p_filesz;
        s.address   = ph.p_vaddr;
        s.addressEnd = ph.p_vaddr + ph.p_memsz;
        switch (ph.p_flags) {
            case 1: case 3: case 5: case 7: execList.push_back(s); break;
            case 2: case 4: case 6:         dataList.push_back(s); break;
        }
    }
    auto* sh = new SectionHelper(this, mc, tc, metadataUsagesCount, ic);
    sh->isElf = true;
    sh->setSection(SearchSectionType::Exec, execList);
    sh->setSection(SearchSectionType::Data, dataList);
    sh->setSection(SearchSectionType::Bss,  dataList);
    return sh;
}

void ElfReader64::load() {
    seek(0);
    memcpy(&ehdr, buf.data(), sizeof(ehdr));
    phdrs.resize(ehdr.e_phnum);
    memcpy(phdrs.data(), buf.data()+ehdr.e_phoff, ehdr.e_phnum*sizeof(Elf64_Phdr));
    computeLoadBase();
    if (isDumped) fixedProgramSegment();

    for (auto& ph : phdrs) if (ph.p_type == PT_DYNAMIC) { ptDynamic = ph; break; }
    size_t dynCount = ptDynamic.p_filesz / sizeof(Elf64_Dyn);
    dynSec.resize(dynCount);
    memcpy(dynSec.data(), buf.data()+ptDynamic.p_offset, dynCount*sizeof(Elf64_Dyn));
    if (isDumped) fixedDynamicSection();

    readSymbol();
    if (!isDumped) {
        relocationProcessing();
        if (checkProtection()) printf("ERROR: This file may be protected.\n");
    }
}

bool ElfReader64::checkSection() {
    try {
        if (ehdr.e_shoff == 0 || ehdr.e_shnum == 0) return false;
        shdrs.resize(ehdr.e_shnum);
        memcpy(shdrs.data(), buf.data()+ehdr.e_shoff, ehdr.e_shnum*sizeof(Elf64_Shdr));
        auto& shstr = shdrs[ehdr.e_shstrndx];
        for (auto& s : shdrs) {
            std::string name = readStringToNull(shstr.sh_offset + s.sh_name);
            if (name == ".text") return true;
        }
    } catch (...) {}
    return false;
}

void ElfReader64::readSymbol() {
    try {
        uint32_t symbolCount = 0;
        uint64_t hashAddr;
        if (dynFind(dynSec, MY_DT_HASH, hashAddr)) {
            uint64_t off = mapVATR(hashAddr);
            seek(off+4);
            symbolCount = ru32();
        } else {
            uint64_t gnuHashAddr = dynVal(dynSec, MY_DT_GNU_HASH);
            uint64_t off = mapVATR(gnuHashAddr);
            seek(off);
            uint32_t nbuckets   = ru32();
            uint32_t symoffset  = ru32();
            uint32_t bloom_size = ru32();
            ru32();
            uint64_t bucketsAddr = off + 16 + 8*bloom_size;
            seek(bucketsAddr);
            uint32_t maxBucket = 0;
            for (uint32_t i = 0; i < nbuckets; i++) {
                uint32_t b = ru32(); if (b > maxBucket) maxBucket = b;
            }
            if (maxBucket < symoffset) {
                symbolCount = symoffset;
            } else {
                seek(bucketsAddr + 4*nbuckets + (maxBucket-symoffset)*4);
                uint32_t last = maxBucket;
                while (true) { uint32_t ce = ru32(); ++last; if (ce & 1) break; }
                symbolCount = last;
            }
        }
        uint64_t dynsymOff = mapVATR(dynVal(dynSec, MY_DT_SYMTAB));
        symTab.resize(symbolCount);
        memcpy(symTab.data(), buf.data()+dynsymOff, symbolCount*sizeof(Elf64_Sym));
    } catch (...) {}
}

static int64_t readSleb128(BinaryStream* stream) {
    int64_t result = 0;
    uint32_t shift = 0;
    while (true) {
        uint8_t byte = stream->ru8();
        int64_t low = (int64_t)(byte & 0x7F);
        result |= low << shift;
        shift += 7;
        if ((byte & 0x80) == 0) {
            if (shift < 64 && (byte & 0x40) != 0)
                result |= -((int64_t)((uint64_t)1 << shift));
            return result;
        }
        if (shift >= 64) return result;
    }
}

static const uint64_t RELOCATION_GROUPED_BY_INFO_FLAG       = 1;
static const uint64_t RELOCATION_GROUPED_BY_OFFSET_DELTA_FLAG = 2;
static const uint64_t RELOCATION_GROUPED_BY_ADDEND_FLAG     = 4;
static const uint64_t RELOCATION_GROUP_HAS_ADDEND_FLAG      = 8;

void ElfReader64::processAndroidPackedRelocations(uint64_t relaOff, uint64_t relaSize) {
    if (relaSize < 4) return;
    uint64_t tableOff = mapVATR(relaOff);
    if (tableOff + 4 > length()) return;
    if (buf[tableOff] != 'A' || buf[tableOff+1] != 'P' ||
        buf[tableOff+2] != 'S' || buf[tableOff+3] != '2') return;

    printf("Detected APS2 packed relocations.\n");
    seek(tableOff + 4);

    int64_t groupCount = readSleb128(this);
    if (groupCount <= 0) return;
    int64_t offset = readSleb128(this);
    int64_t addend = 0;
    bool isAarch64 = (ehdr.e_machine == EM_AARCH64);
    bool isX86_64  = (ehdr.e_machine == EM_X86_64);

    int total = 0, applied = 0, unrecognized = 0, mapFailed = 0;

    for (int64_t g = 0; g < groupCount; g++) {
        int64_t groupSize = readSleb128(this);
        if (groupSize <= 0) break;
        int64_t groupFlags = readSleb128(this);
        int64_t groupOffsetDelta = 0;
        if (groupFlags & RELOCATION_GROUPED_BY_OFFSET_DELTA_FLAG)
            groupOffsetDelta = readSleb128(this);
        int64_t groupRInfo = 0;
        if (groupFlags & RELOCATION_GROUPED_BY_INFO_FLAG)
            groupRInfo = readSleb128(this);

        if ((groupFlags & RELOCATION_GROUPED_BY_ADDEND_FLAG) &&
            (groupFlags & RELOCATION_GROUP_HAS_ADDEND_FLAG))
            addend += readSleb128(this);
        else if (!(groupFlags & RELOCATION_GROUP_HAS_ADDEND_FLAG))
            addend = 0;

        for (int64_t i = 0; i < groupSize; i++) {
            if (groupFlags & RELOCATION_GROUPED_BY_OFFSET_DELTA_FLAG)
                offset += groupOffsetDelta;
            else
                offset += readSleb128(this);

            int64_t rInfo;
            if (groupFlags & RELOCATION_GROUPED_BY_INFO_FLAG) {
                rInfo = groupRInfo;
            } else {
                rInfo = readSleb128(this);
                groupRInfo = rInfo;
            }

            if ((groupFlags & RELOCATION_GROUP_HAS_ADDEND_FLAG)) {
                if (!(groupFlags & RELOCATION_GROUPED_BY_ADDEND_FLAG))
                    addend += readSleb128(this);
            } else {
                addend = 0;
            }

            total++;
            uint32_t relType = (uint32_t)(rInfo & 0xFFFFFFFFLL);
            size_t symIdx = (size_t)(((uint64_t)rInfo) >> 32);
            uint64_t rOffset = (uint64_t)offset;
            int64_t rAddend = addend;

            uint64_t value = 0;
            bool ok = false;
            if (isAarch64) {
                if (relType == R_AARCH64_ABS64 && symIdx < symTab.size()) {
                    value = symTab[symIdx].st_value + rAddend; ok = true;
                } else if (relType == R_AARCH64_RELATIVE) {
                    value = (uint64_t)rAddend; ok = true;
                }
            } else if (isX86_64) {
                if (relType == MY_R_X86_64_64 && symIdx < symTab.size()) {
                    value = symTab[symIdx].st_value + rAddend; ok = true;
                } else if (relType == MY_R_X86_64_RELATIVE) {
                    value = (uint64_t)rAddend; ok = true;
                }
            }

            if (ok) {
                try {
                    size_t dstOff = (size_t)mapVATR(rOffset);
                    memcpy(buf.data() + dstOff, &value, 8);
                    applied++;
                } catch (...) {
                    mapFailed++;
                }
            } else {
                unrecognized++;
            }
        }
    }
    printf("APS2: total=%d applied=%d unrecognized=%d map_failed=%d\n",
           total, applied, unrecognized, mapFailed);
}

void ElfReader64::relocationProcessing() {
    printf("Applying relocations...\n");
    try {
        uint64_t relaOff, relaSize;

        // Try Android-specific packed relocations first (CODM APS2)
        if (dynFind(dynSec, MY_DT_ANDROID_RELA, relaOff) &&
            dynFind(dynSec, MY_DT_ANDROID_RELASZ, relaSize)) {
            // Check if it's APS2 format
            uint64_t tableOff = mapVATR(relaOff);
            if (tableOff + 8 <= length() &&
                buf[tableOff] == 'A' && buf[tableOff+1] == 'P' &&
                buf[tableOff+2] == 'S' && buf[tableOff+3] == '2') {
                processAndroidPackedRelocations(relaOff, relaSize);
                return;
            }
            // Otherwise fall through to standard processing
        }

        // Standard DT_ANDROID_RELA or DT_RELA
        if (!dynFind(dynSec, MY_DT_ANDROID_RELA, relaOff) ||
            !dynFind(dynSec, MY_DT_ANDROID_RELASZ, relaSize)) {
            relaOff  = dynVal(dynSec, MY_DT_RELA);
            relaSize = dynVal(dynSec, MY_DT_RELASZ);
        }
        size_t count = (size_t)relaSize / sizeof(Elf64_Rela);
        std::vector<Elf64_Rela> relas(count);
        uint64_t fileOff = mapVATR(relaOff);
        memcpy(relas.data(), buf.data()+fileOff, count*sizeof(Elf64_Rela));
        for (auto& r : relas) {
            uint64_t type = r.r_info & 0xffffffff;
            uint64_t sym  = r.r_info >> 32;
            uint64_t value = 0; bool ok = false;
            if (ehdr.e_machine == EM_AARCH64) {
                if (type == R_AARCH64_ABS64)      { value = symTab[sym].st_value + r.r_addend; ok=true; }
                else if (type == R_AARCH64_RELATIVE){ value = r.r_addend; ok=true; }
            } else if (ehdr.e_machine == EM_X86_64) {
                if (type == MY_R_X86_64_64)           { value = symTab[sym].st_value + r.r_addend; ok=true; }
                else if (type == MY_R_X86_64_RELATIVE) { value = r.r_addend; ok=true; }
            }
            if (ok) {
                size_t dstOff = (size_t)mapVATR(r.r_offset);
                memcpy(buf.data()+dstOff, &value, 8);
            }
        }
    } catch (...) {}
}

bool ElfReader64::checkProtection() {
    try {
        uint64_t initAddr;
        if (dynFind(dynSec, MY_DT_INIT, initAddr)) {
            printf("WARNING: find .init_proc\n"); return true;
        }
        uint64_t dynstrOff = mapVATR(dynVal(dynSec, MY_DT_STRTAB));
        for (auto& sym : symTab) {
            std::string name = readStringToNull(dynstrOff + sym.st_name);
            if (name == "JNI_OnLoad") { printf("WARNING: find JNI_OnLoad\n"); return true; }
        }
        if (!shdrs.empty())
            for (auto& s : shdrs)
                if (s.sh_type == MY_SHT_LOUSER) { printf("WARNING: find MY_SHT_LOUSER\n"); return true; }
    } catch (...) {}
    return false;
}

void ElfReader64::fixedProgramSegment() {
    for (size_t i = 0; i < phdrs.size(); i++) {
        size_t off = (size_t)ehdr.e_phoff + i*sizeof(Elf64_Phdr) + 8;
        phdrs[i].p_offset = phdrs[i].p_vaddr;
        memcpy(buf.data()+off, &phdrs[i].p_offset, 8);
        phdrs[i].p_vaddr += imageBase;
        memcpy(buf.data()+off+8, &phdrs[i].p_vaddr, 8);
        off += 24;
        phdrs[i].p_filesz = phdrs[i].p_memsz;
        memcpy(buf.data()+off, &phdrs[i].p_filesz, 8);
    }
}

void ElfReader64::fixedDynamicSection() {
    for (size_t i = 0; i < dynSec.size(); i++) {
        size_t off = (size_t)ptDynamic.p_offset + i*sizeof(Elf64_Dyn) + 8;
        uint64_t tag = (uint64_t)dynSec[i].d_tag;
        if (tag==MY_DT_PLTGOT||tag==MY_DT_HASH||tag==MY_DT_STRTAB||tag==MY_DT_SYMTAB||
            tag==MY_DT_RELA||tag==MY_DT_INIT||tag==MY_DT_FINI||tag==MY_DT_REL||
            tag==MY_DT_JMPREL||tag==MY_DT_INIT_ARRAY||tag==MY_DT_FINI_ARRAY) {
            dynSec[i].d_un.d_val += imageBase;
            memcpy(buf.data()+off, &dynSec[i].d_un.d_val, 8);
        }
    }
}

bool ElfReader64::plusSearch(int mc, int tc, int ic) {
    auto* sh = getSectionHelper(mc, tc, ic);
    uint64_t cr = sh->findCodeRegistration();
    uint64_t mr = sh->findMetadataRegistration();
    delete sh;
    return autoPlusInit(cr, mr);
}

bool ElfReader64::symbolSearch() {
    uint64_t cr=0, mr=0;
    uint64_t dynstrOff = mapVATR(dynVal(dynSec, MY_DT_STRTAB));
    for (auto& sym : symTab) {
        std::string name = readStringToNull(dynstrOff + sym.st_name);
        if (name == "g_CodeRegistration")     cr = sym.st_value;
        if (name == "g_MetadataRegistration") mr = sym.st_value;
    }
    if (cr && mr) {
        printf("Detected Symbol !\n");
        printf("CodeRegistration : %llx\n", (unsigned long long)cr);
        printf("MetadataRegistration : %llx\n", (unsigned long long)mr);
        init(cr, mr);
        return true;
    }
    printf("ERROR: No symbol is detected\n");
    return false;
}

SectionHelper* ElfReader64::getSectionHelper(int mc, int tc, int ic) {
    std::vector<SearchSection> dataList, execList;
    for (auto& ph : phdrs) {
        if (ph.p_memsz == 0) continue;
        SearchSection s;
        s.offset    = ph.p_offset;
        s.offsetEnd = ph.p_offset + ph.p_filesz;
        s.address   = ph.p_vaddr;
        s.addressEnd = ph.p_vaddr + ph.p_memsz;
        switch (ph.p_flags) {
            case 1: case 3: case 5: case 7: execList.push_back(s); break;
            case 2: case 4: case 6:         dataList.push_back(s); break;
        }
    }
    auto* sh = new SectionHelper(this, mc, tc, metadataUsagesCount, ic);
    sh->isElf = true;
    sh->setSection(SearchSectionType::Exec, execList);
    sh->setSection(SearchSectionType::Data, dataList);
    sh->setSection(SearchSectionType::Bss,  dataList);
    return sh;
}

Il2CppBase* openElf(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) throw std::runtime_error(std::string("Cannot open ELF: ") + path);
    unsigned char ident[16];
    fread(ident, 1, 16, f);
    fclose(f);
    if (ident[EI_CLASS] == ELFCLASS32) return new ElfReader32(path);
    return new ElfReader64(path);
}
