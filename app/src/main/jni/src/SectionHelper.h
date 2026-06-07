#pragma once
#include "BinaryStream.h"
#include <vector>
#include <cstdint>

class Il2CppBase;

struct SearchSection {
    uint64_t offset    = 0;
    uint64_t offsetEnd = 0;
    uint64_t address   = 0;
    uint64_t addressEnd = 0;
};

enum class SearchSectionType { Exec, Data, Bss };

class SectionHelper {
public:
    std::vector<SearchSection> exec;
    std::vector<SearchSection> data;
    std::vector<SearchSection> bss;

    Il2CppBase* il2Cpp;
    int     methodCount;
    int     typeDefinitionsCount;
    int64_t metadataUsagesCount;
    int     imageCount;
    bool    pointerInExec = false;
    bool    isElf = true;

    SectionHelper(Il2CppBase* il, int mc, int tc, int64_t muc, int ic)
        : il2Cpp(il), methodCount(mc), typeDefinitionsCount(tc),
          metadataUsagesCount(muc), imageCount(ic) {}

    void setSection(SearchSectionType type, std::vector<SearchSection> secs) {
        switch (type) {
            case SearchSectionType::Exec: exec = std::move(secs); break;
            case SearchSectionType::Data: data = std::move(secs); break;
            case SearchSectionType::Bss:  bss  = std::move(secs); break;
        }
    }

    uint64_t findCodeRegistration();
    uint64_t findMetadataRegistration();

private:
    uint64_t findCodeRegistrationOld();
    uint64_t findMetadataRegistrationOld();
    uint64_t findMetadataRegistrationV21();
    uint64_t findCodeRegistrationData() { return findCodeRegistration2019(data); }
    uint64_t findCodeRegistrationExec() { return findCodeRegistration2019(exec); }
    uint64_t findCodeRegistration2019(std::vector<SearchSection>& secs);
    std::vector<uint64_t> findReference(uint64_t addr);
    bool checkPointerRangeDataRa(uint64_t pointer);
    bool checkPointerRangeExecVa(const std::vector<uint64_t>& ptrs);
    bool checkPointerRangeDataVa(const std::vector<uint64_t>& ptrs);
    bool checkPointerRangeBssVa(const std::vector<uint64_t>& ptrs);
};
