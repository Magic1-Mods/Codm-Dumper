#include "SectionHelper.h"
#include "Il2CppBase.h"
#include <algorithm>
#include <cstring>

static const uint8_t featureBytes[] = {
    0x6D,0x73,0x63,0x6F,0x72,0x6C,0x69,0x62,0x2E,0x64,0x6C,0x6C,0x00
};
static const size_t featureBytesLen = sizeof(featureBytes);

uint64_t SectionHelper::findCodeRegistration() {
    if (il2Cpp->version >= 24.2) {
        uint64_t codeReg;
        if (isElf) {
            // ELF: exec-first, data-second (CODM-specific order)
            codeReg = findCodeRegistrationExec();
            if (codeReg == 0) {
                codeReg = findCodeRegistrationData();
            } else {
                pointerInExec = true;
            }
        } else {
            // Non-ELF (Macho, PE): data-first, exec-second
            codeReg = findCodeRegistrationData();
            if (codeReg == 0) {
                codeReg = findCodeRegistrationExec();
                pointerInExec = true;
            }
        }
        return codeReg;
    }
    return findCodeRegistrationOld();
}

uint64_t SectionHelper::findMetadataRegistration() {
    if (il2Cpp->version < 19.0) return 0;
    if (il2Cpp->version >= 27.0) return findMetadataRegistrationV21();
    return findMetadataRegistrationOld();
}

uint64_t SectionHelper::findCodeRegistrationOld() {
    uint64_t ptrSz = il2Cpp->ptrSize();
    for (auto& sec : data) {
        il2Cpp->seek(sec.offset);
        while (il2Cpp->tell() < sec.offsetEnd) {
            uint64_t addr = il2Cpp->tell();
            int64_t v = il2Cpp->riptr();
            if (v == methodCount) {
                try {
                    uint64_t pointer = il2Cpp->mapVATR(il2Cpp->ruptr());
                    if (checkPointerRangeDataRa(pointer)) {
                        il2Cpp->seek(pointer);
                        std::vector<uint64_t> ptrs((size_t)methodCount);
                        for (int i = 0; i < methodCount; i++) ptrs[i] = il2Cpp->ruptr();
                        if (checkPointerRangeExecVa(ptrs)) {
                            return addr - sec.offset + sec.address;
                        }
                    }
                } catch (...) {}
            }
            il2Cpp->seek(addr + ptrSz);
        }
    }
    return 0;
}

uint64_t SectionHelper::findMetadataRegistrationOld() {
    uint64_t ptrSz = il2Cpp->ptrSize();
    for (auto& sec : data) {
        il2Cpp->seek(sec.offset);
        uint64_t end = std::min(sec.offsetEnd, il2Cpp->length()) - ptrSz;
        while (il2Cpp->tell() < end) {
            uint64_t addr = il2Cpp->tell();
            int64_t v = il2Cpp->riptr();
            if (v == typeDefinitionsCount) {
                try {
                    il2Cpp->seek(il2Cpp->tell() + ptrSz * 2);
                    uint64_t pointer = il2Cpp->mapVATR(il2Cpp->ruptr());
                    if (checkPointerRangeDataRa(pointer)) {
                        il2Cpp->seek(pointer);
                        std::vector<uint64_t> ptrs((size_t)metadataUsagesCount);
                        for (int64_t i = 0; i < metadataUsagesCount; i++) ptrs[i] = il2Cpp->ruptr();
                        if (checkPointerRangeBssVa(ptrs)) {
                            return addr - ptrSz * 12 - sec.offset + sec.address;
                        }
                    }
                } catch (...) {}
            }
            il2Cpp->seek(addr + ptrSz);
        }
    }
    return 0;
}

uint64_t SectionHelper::findMetadataRegistrationV21() {
    uint64_t ptrSz = il2Cpp->ptrSize();
    for (auto& sec : data) {
        uint64_t end = std::min(sec.offsetEnd, il2Cpp->length());
        if (end < ptrSz * 4) continue;
        end -= ptrSz * 4;
        uint64_t pos = sec.offset;
        while (pos < end) {
            il2Cpp->seek(pos);
            int64_t v = il2Cpp->riptr();
            if (v == typeDefinitionsCount) {
                il2Cpp->ruptr();
                int64_t v2 = il2Cpp->riptr();
                if (v2 == typeDefinitionsCount) {
                    try {
                        uint64_t rawPtr = il2Cpp->ruptr();
                        uint64_t pointer = il2Cpp->mapVATR(rawPtr);
                        if (checkPointerRangeDataRa(pointer)) {
                            il2Cpp->seek(pointer);
                            std::vector<uint64_t> ptrs((size_t)typeDefinitionsCount);
                            for (int i = 0; i < typeDefinitionsCount; i++)
                                ptrs[i] = il2Cpp->ruptr();
                            // Use pointerInExec to decide which range to check (CODM improvement)
                            bool flag;
                            if (pointerInExec) {
                                flag = checkPointerRangeExecVa(ptrs);
                            } else {
                                flag = checkPointerRangeDataVa(ptrs);
                            }
                            if (flag) {
                                return pos - ptrSz * 10 - sec.offset + sec.address;
                            }
                        }
                    } catch (...) {}
                }
            }
            pos += ptrSz;
        }
    }
    return 0;
}

uint64_t SectionHelper::findCodeRegistration2019(std::vector<SearchSection>& secs) {
    uint64_t ptrSz = il2Cpp->ptrSize();
    for (auto& sec : secs) {
        size_t secSize = (size_t)(sec.offsetEnd - sec.offset);
        auto results = il2Cpp->searchBytes(featureBytes, featureBytesLen,
                                           (size_t)sec.offset,
                                           (size_t)sec.offset + secSize);
        for (size_t resIdx : results) {
            uint64_t dllva = (uint64_t)(resIdx - sec.offset) + sec.address;
            for (uint64_t refva : findReference(dllva)) {
                for (uint64_t refva2 : findReference(refva)) {
                    if (il2Cpp->version >= 27.0) {
                        for (int i = imageCount-1; i >= 0; i--) {
                            for (uint64_t refva3 : findReference(refva2 - (uint64_t)i * ptrSz)) {
                                try {
                                    uint64_t checkPos = il2Cpp->mapVATR(refva3 - ptrSz);
                                    il2Cpp->seek(checkPos);
                                    int64_t count = il2Cpp->riptr();
                                    if (count == imageCount) {
                                        if (il2Cpp->version >= 29.0)
                                            return refva3 - ptrSz * 14;
                                        return refva3 - ptrSz * 13;
                                    }
                                } catch (...) {}
                            }
                        }
                    } else {
                        for (int i = 0; i < imageCount; i++) {
                            for (uint64_t refva3 : findReference(refva2 - (uint64_t)i * ptrSz)) {
                                return refva3 - ptrSz * 13;
                            }
                        }
                    }
                }
            }
        }
    }
    return 0;
}

std::vector<uint64_t> SectionHelper::findReference(uint64_t addr) {
    std::vector<uint64_t> results;
    uint64_t ptrSz = il2Cpp->ptrSize();
    for (auto& sec : data) {
        uint64_t p = sec.offset;
        uint64_t end = std::min(sec.offsetEnd, il2Cpp->length());
        if (end < ptrSz) continue;
        end -= ptrSz;
        while (p < end) {
            il2Cpp->seek(p);
            uint64_t v = il2Cpp->ruptr();
            if (v == addr) {
                results.push_back(p - sec.offset + sec.address);
            }
            p += ptrSz;
        }
    }
    return results;
}

bool SectionHelper::checkPointerRangeDataRa(uint64_t pointer) {
    for (auto& s : data)
        if (pointer >= s.offset && pointer <= s.offsetEnd) return true;
    return false;
}
bool SectionHelper::checkPointerRangeExecVa(const std::vector<uint64_t>& ptrs) {
    bool any = false;
    for (auto p : ptrs) {
        if (p == 0 || p == UINTPTR_MAX) continue; // skip zero/invalid method pointers
        any = true;
        bool found = false;
        for (auto& s : exec) if (p >= s.address && p <= s.addressEnd) { found=true; break; }
        if (!found) return false;
    }
    return any;
}
bool SectionHelper::checkPointerRangeDataVa(const std::vector<uint64_t>& ptrs) {
    bool any = false;
    for (auto p : ptrs) {
        if (p == 0) continue;
        any = true;
        bool found = false;
        for (auto& s : data) if (p >= s.address && p <= s.addressEnd) { found=true; break; }
        if (!found) return false;
    }
    return any;
}
bool SectionHelper::checkPointerRangeBssVa(const std::vector<uint64_t>& ptrs) {
    bool any = false;
    for (auto p : ptrs) {
        if (p == 0) continue; // skip zero pointers
        any = true;
        bool found = false;
        for (auto& s : bss) if (p >= s.address && p <= s.addressEnd) { found=true; break; }
        if (!found) return false;
    }
    return any;
}
