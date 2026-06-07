#pragma once
#include "BinaryStream.h"
#include "Il2CppStructs.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <map>
#include <stdexcept>

class Metadata : public BinaryStream {
public:
    Il2CppGlobalMetadataHeader header;
    std::vector<Il2CppImageDefinition>          imageDefs;
    std::vector<Il2CppAssemblyDefinition>       assemblyDefs;
    std::vector<Il2CppTypeDefinition>           typeDefs;
    std::vector<Il2CppMethodDefinition>         methodDefs;
    std::vector<Il2CppParameterDefinition>      parameterDefs;
    std::vector<Il2CppFieldDefinition>          fieldDefs;
    std::vector<Il2CppPropertyDefinition>       propertyDefs;
    std::vector<Il2CppCustomAttributeTypeRange> attributeTypeRanges;
    std::vector<Il2CppCustomAttributeDataRange> attributeDataRanges;
    std::vector<Il2CppStringLiteral>            stringLiterals;
    std::vector<int32_t>                        attributeTypes;
    std::vector<int32_t>                        interfaceIndices;
    std::unordered_map<Il2CppMetadataUsage,
        std::map<uint32_t, uint32_t>>           metadataUsageDic;
    int64_t                                     metadataUsagesCount = 0;
    std::vector<int32_t>                        nestedTypeIndices;
    std::vector<Il2CppEventDefinition>          eventDefs;
    std::vector<Il2CppGenericContainer>         genericContainers;
    std::vector<Il2CppFieldRef>                 fieldRefs;
    std::vector<Il2CppGenericParameter>         genericParameters;
    std::vector<int32_t>                        constraintIndices;
    std::vector<uint32_t>                       vtableMethods;
    std::vector<Il2CppRGCTXDefinition>          rgctxEntries;

    uint64_t ImageBase = 0;

private:
    std::unordered_map<int32_t, Il2CppFieldDefaultValue>     fieldDefaultValuesDic;
    std::unordered_map<int32_t, Il2CppParameterDefaultValue> parameterDefaultValuesDic;
    std::unordered_map<uint32_t, std::string> stringCache;

    template<typename T, typename ReadFn>
    std::vector<T> readArray(uint64_t offset, int32_t byteSize, size_t itemSize, ReadFn fn) {
        if (byteSize <= 0 || itemSize == 0) return {};
        size_t count = (size_t)byteSize / itemSize;
        std::vector<T> v;
        v.reserve(count);
        seek(offset);
        for (size_t i = 0; i < count; i++) v.push_back(fn(*this));
        return v;
    }

    std::vector<Il2CppMetadataUsageList>  usageLists;
    std::vector<Il2CppMetadataUsagePair>  usagePairs;

    void processingMetadataUsage();

public:
    explicit Metadata(const char* path);

    bool getFieldDefaultValue(int32_t index, Il2CppFieldDefaultValue& out) const {
        auto it = fieldDefaultValuesDic.find(index);
        if (it == fieldDefaultValuesDic.end()) return false;
        out = it->second; return true;
    }
    bool getParameterDefaultValue(int32_t index, Il2CppParameterDefaultValue& out) const {
        auto it = parameterDefaultValuesDic.find(index);
        if (it == parameterDefaultValuesDic.end()) return false;
        out = it->second; return true;
    }
    uint32_t getDefaultValueFromIndex(int32_t index) const {
        return (uint32_t)(header.fieldAndParameterDefaultValueDataOffset + index);
    }
    const std::string& getStringFromIndex(uint32_t index);
    std::string getStringLiteralFromIndex(uint32_t index);
    int32_t getCustomAttributeIndex(const Il2CppImageDefinition& imageDef,
                                    int32_t customAttributeIndex, uint32_t token) const;
    static uint32_t getEncodedIndexType(uint32_t index) { return (index & 0xE0000000u) >> 29; }
    uint32_t getDecodedMethodIndex(uint32_t index) const {
        if (version >= 27.0) return (index & 0x1FFFFFFEu) >> 1;
        return index & 0x1FFFFFFFu;
    }
};
