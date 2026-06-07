#pragma once
#include "Il2CppBase.h"
#include "SectionHelper.h"
#include <elf.h>
#include <vector>
#include <string>
#include <stdexcept>
#include <algorithm>

class ElfReaderBase : public Il2CppBase {
public:
    virtual void reload() = 0;
    bool checkDump() override { return !checkSection(); }
protected:
    virtual bool checkSection() = 0;
};

class ElfReader32 : public ElfReaderBase {
    Elf32_Ehdr   ehdr{};
    std::vector<Elf32_Phdr> phdrs;
    std::vector<Elf32_Dyn>  dynSec;
    std::vector<Elf32_Sym>  symTab;
    std::vector<Elf32_Shdr> shdrs;
    Elf32_Phdr   ptDynamic{};
    uint32_t     loadBase = 0;

public:
    explicit ElfReader32(const char* path) {
        is32bit = true;
        loadFile(path);
        load();
    }
    void reload() override { load(); }

    uint64_t mapVATR(uint64_t addr) override {
        uint64_t clean = addr & ~1ULL;
        for (auto& ph : phdrs) {
            if (ph.p_type != PT_LOAD) continue;
            if (clean >= ph.p_vaddr && clean < ph.p_vaddr + ph.p_filesz)
                return clean - ph.p_vaddr + ph.p_offset;
        }
        throw std::out_of_range("ELF32 VATR not found");
    }
    uint64_t mapRTVA(uint64_t addr) override {
        for (auto& ph : phdrs) {
            if (ph.p_type != PT_LOAD) continue;
            if (addr >= ph.p_offset && addr < ph.p_offset + ph.p_filesz)
                return addr - ph.p_offset + ph.p_vaddr;
        }
        return 0;
    }
    bool search() override;
    bool plusSearch(int methodCount, int typeDefinitionsCount, int imageCount) override;
    bool symbolSearch() override;
    SectionHelper* getSectionHelper(int methodCount, int typeDefinitionsCount, int imageCount) override;

    uint64_t getRVA(uint64_t pointer) override {
        if (isDumped) return pointer - imageBase;
        uint64_t clean = pointer & ~1ULL;
        return clean - (uint64_t)loadBase;
    }

private:
    void load();
    void computeLoadBase() {
        loadBase = 0xFFFFFFFFu;
        for (auto& ph : phdrs)
            if (ph.p_type == PT_LOAD && ph.p_vaddr < loadBase)
                loadBase = ph.p_vaddr;
        if (loadBase == 0xFFFFFFFFu) loadBase = 0;
    }
    bool checkSection() override;
    void readSymbol();
    void relocationProcessing();
    bool checkProtection();
    void fixedProgramSegment();
    void fixedDynamicSection();
};

class ElfReader64 : public ElfReaderBase {
    Elf64_Ehdr   ehdr{};
    std::vector<Elf64_Phdr> phdrs;
    std::vector<Elf64_Dyn>  dynSec;
    std::vector<Elf64_Sym>  symTab;
    std::vector<Elf64_Shdr> shdrs;
    Elf64_Phdr   ptDynamic{};
    uint64_t     loadBase = 0;

public:
    explicit ElfReader64(const char* path) {
        is32bit = false;
        loadFile(path);
        load();
    }
    void reload() override { load(); }

    uint64_t mapVATR(uint64_t addr) override {
        for (auto& ph : phdrs) {
            if (ph.p_type != PT_LOAD) continue;
            if (addr >= ph.p_vaddr && addr < ph.p_vaddr + ph.p_filesz)
                return addr - ph.p_vaddr + ph.p_offset;
        }
        throw std::out_of_range("ELF64 VATR not found");
    }
    uint64_t mapRTVA(uint64_t addr) override {
        for (auto& ph : phdrs) {
            if (ph.p_type != PT_LOAD) continue;
            if (addr >= ph.p_offset && addr < ph.p_offset + ph.p_filesz)
                return addr - ph.p_offset + ph.p_vaddr;
        }
        return 0;
    }
    bool search() override { return false; }
    bool plusSearch(int methodCount, int typeDefinitionsCount, int imageCount) override;
    bool symbolSearch() override;
    SectionHelper* getSectionHelper(int methodCount, int typeDefinitionsCount, int imageCount) override;

    uint64_t getRVA(uint64_t pointer) override {
        if (isDumped) return pointer - imageBase;
        return pointer - loadBase;
    }

private:
    void load();
    void computeLoadBase() {
        loadBase = UINT64_MAX;
        for (auto& ph : phdrs)
            if (ph.p_type == PT_LOAD && ph.p_vaddr < loadBase)
                loadBase = ph.p_vaddr;
        if (loadBase == UINT64_MAX) loadBase = 0;
    }
    bool checkSection() override;
    void readSymbol();
    void relocationProcessing();
    void processAndroidPackedRelocations(uint64_t relaOff, uint64_t relaSize);
    bool checkProtection();
    void fixedProgramSegment();
    void fixedDynamicSection();
};

Il2CppBase* openElf(const char* path);
