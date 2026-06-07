#pragma once
#include "BinaryStream.h"
#include "Il2CppStructs.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <stdexcept>
#include <algorithm>
#include <functional>

class SectionHelper;

class Il2CppBase : public BinaryStream {
protected:
    Il2CppCodeRegistration  pCodeReg;
    Il2CppMetadataRegistration pMetaReg;
    bool fieldOffsetsArePointers = false;
    std::vector<uint64_t> fieldOffsets;
    int64_t metadataUsagesCount = 0;

public:
    std::vector<uint64_t>  methodPointers;
    std::vector<uint64_t>  genericMethodPointers;
    std::vector<uint64_t>  invokerPointers;
    std::vector<uint64_t>  customAttributeGenerators;
    std::vector<uint64_t>  reversePInvokeWrappers;
    std::vector<uint64_t>  unresolvedVirtualCallPointers;
    std::vector<Il2CppType> types;
    std::unordered_map<uint64_t, size_t> typeDic;
    std::vector<uint64_t>  metadataUsages;
    std::vector<uint64_t>  genericInstPointers;
    std::vector<Il2CppGenericInst> genericInsts;
    std::vector<Il2CppMethodSpec>  methodSpecs;
    std::unordered_map<int32_t, std::vector<Il2CppMethodSpec>> methodDefinitionMethodSpecs;
    std::unordered_map<size_t, uint64_t>  methodSpecGenericMethodPointers;
    std::unordered_map<std::string, Il2CppCodeGenModule>        codeGenModules;
    std::unordered_map<std::string, std::vector<uint64_t>>      codeGenModuleMethodPointers;
    std::unordered_map<std::string,
        std::unordered_map<uint32_t, std::vector<Il2CppRGCTXDefinition>>> rgctxsDictionary;

    virtual ~Il2CppBase() = default;

    virtual uint64_t mapVATR(uint64_t addr) = 0;
    virtual uint64_t mapRTVA(uint64_t addr) = 0;
    virtual bool search() = 0;
    virtual bool plusSearch(int methodCount, int typeDefinitionsCount, int imageCount) = 0;
    virtual bool symbolSearch() = 0;
    virtual SectionHelper* getSectionHelper(int methodCount, int typeDefinitionsCount, int imageCount) = 0;
    virtual bool checkDump() = 0;
    virtual uint64_t getRVA(uint64_t pointer) { return pointer; }

    void setProperties(double ver, int64_t usagesCount) {
        version = ver;
        metadataUsagesCount = usagesCount;
    }

    // Ported from Project B: normalize pointer through VA/offset round-trip for type dict
    uint64_t normalizePointer(uint64_t pointer) {
        try {
            return mapRTVA(mapVATR(pointer));
        } catch (...) {
            return pointer;
        }
    }

    template<typename T>
    T mapVATR_struct(uint64_t addr) {
        seek(mapVATR(addr));
        return readStruct<T>();
    }

    template<typename T>
    std::vector<T> mapVATR_array(uint64_t addr, int64_t count);

    std::vector<uint64_t> mapVATR_ptrs(uint64_t addr, int64_t count) {
        seek(mapVATR(addr));
        std::vector<uint64_t> v((size_t)count);
        for (int64_t i = 0; i < count; i++) v[i] = ruptr();
        return v;
    }
    std::vector<uint32_t> mapVATR_u32arr(uint64_t addr, int64_t count) {
        seek(mapVATR(addr));
        return readPODArray<uint32_t>(tell(), (size_t)count);
    }

    std::string mapVATR_string(uint64_t addr) {
        return readStringToNull(mapVATR(addr));
    }

    Il2CppType* getIl2CppType(uint64_t pointer) {
        auto it = typeDic.find(pointer);
        if (it == typeDic.end()) {
            // Try normalized pointer (Project B improvement)
            uint64_t normPtr = normalizePointer(pointer);
            if (normPtr != pointer) {
                it = typeDic.find(normPtr);
            }
        }
        if (it == typeDic.end()) return nullptr;
        return &types[it->second];
    }

    int getFieldOffsetFromIndex(int typeIndex, int fieldIndexInType, int fieldIndex,
                                bool isValueType, bool isStatic) {
        try {
            int offset = -1;
            if (fieldOffsetsArePointers) {
                uint64_t ptr = fieldOffsets[(size_t)typeIndex];
                if (ptr > 0) {
                    seek(mapVATR(ptr) + 4ULL * (uint64_t)fieldIndexInType);
                    offset = ri32();
                }
            } else {
                offset = (int)(uint32_t)fieldOffsets[(size_t)fieldIndex];
            }
            if (offset > 0) {
                if (isValueType && !isStatic) {
                    offset -= (is32bit ? 8 : 16);
                }
            }
            return offset;
        } catch (...) { return -1; }
    }

    uint64_t getMethodPointer(const std::string& imageName, const Il2CppMethodDefinition& methodDef) {
        if (version >= 24.2) {
            uint32_t token = methodDef.token;
            auto it = codeGenModuleMethodPointers.find(imageName);
            if (it == codeGenModuleMethodPointers.end()) return 0;
            uint32_t idx = token & 0x00FFFFFFu;
            if (idx == 0 || idx-1 >= it->second.size()) return 0;
            return it->second[idx-1];
        } else {
            int32_t mi = methodDef.methodIndex;
            if (mi >= 0 && (size_t)mi < methodPointers.size())
                return methodPointers[(size_t)mi];
        }
        return 0;
    }

public:
    bool autoPlusInit(uint64_t codeReg, uint64_t metaReg);
    void init(uint64_t codeReg, uint64_t metaReg);

private:
    template<typename T> T readStruct();
};

template<> inline Il2CppCodeGenModule Il2CppBase::readStruct<Il2CppCodeGenModule>() {
    return Il2CppCodeGenModule::read(*this, version);
}
template<> inline Il2CppGenericInst Il2CppBase::readStruct<Il2CppGenericInst>() {
    return Il2CppGenericInst::read(*this);
}
template<> inline Il2CppGenericClass Il2CppBase::readStruct<Il2CppGenericClass>() {
    return Il2CppGenericClass::read(*this, version);
}
template<> inline Il2CppArrayType Il2CppBase::readStruct<Il2CppArrayType>() {
    return Il2CppArrayType::read(*this);
}
template<> inline Il2CppRGCTXDefinition Il2CppBase::readStruct<Il2CppRGCTXDefinition>() {
    return Il2CppRGCTXDefinition::read(*this, version);
}
template<> inline Il2CppTokenRangePair Il2CppBase::readStruct<Il2CppTokenRangePair>() {
    return Il2CppTokenRangePair::read(*this);
}

template<typename T>
std::vector<T> Il2CppBase::mapVATR_array(uint64_t addr, int64_t count) {
    seek(mapVATR(addr));
    std::vector<T> v;
    v.reserve((size_t)count);
    for (int64_t i = 0; i < count; i++) v.push_back(readStruct<T>());
    return v;
}
