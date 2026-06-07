#include "Metadata.h"
#include <stdexcept>
#include <algorithm>
#include <cstring>

Metadata::Metadata(const char* path) {
    loadFile(path);

    uint32_t sanity = readU32At(0);
    if (sanity != 0xFAB11BAFu) throw std::runtime_error("Not a valid metadata file");
    int32_t rawVer; memcpy(&rawVer, buf.data()+4, 4);
    if (rawVer != 23) throw std::runtime_error("Unsupported metadata version (only v23 supported)");

    version = (double)rawVer;
    header = Il2CppGlobalMetadataHeader::read(*this, version);

    imageDefs = readArray<Il2CppImageDefinition>(
        header.imagesOffset, header.imagesSize,
        Il2CppImageDefinition::structSize(version),
        [&](BinaryStream& s){ return Il2CppImageDefinition::read(s, version); });

    assemblyDefs = readArray<Il2CppAssemblyDefinition>(
        header.assembliesOffset, header.assembliesSize,
        Il2CppAssemblyDefinition::structSize(version),
        [&](BinaryStream& s){ return Il2CppAssemblyDefinition::read(s, version); });

    typeDefs = readArray<Il2CppTypeDefinition>(
        header.typeDefinitionsOffset, header.typeDefinitionsSize,
        Il2CppTypeDefinition::structSize(version),
        [&](BinaryStream& s){ return Il2CppTypeDefinition::read(s, version); });

    methodDefs = readArray<Il2CppMethodDefinition>(
        header.methodsOffset, header.methodsSize,
        Il2CppMethodDefinition::structSize(version),
        [&](BinaryStream& s){ return Il2CppMethodDefinition::read(s, version); });

    parameterDefs = readArray<Il2CppParameterDefinition>(
        header.parametersOffset, header.parametersSize,
        Il2CppParameterDefinition::structSize(version),
        [&](BinaryStream& s){ return Il2CppParameterDefinition::read(s, version); });

    fieldDefs = readArray<Il2CppFieldDefinition>(
        header.fieldsOffset, header.fieldsSize,
        Il2CppFieldDefinition::structSize(version),
        [&](BinaryStream& s){ return Il2CppFieldDefinition::read(s, version); });

    if (header.fieldDefaultValuesOffset + (uint32_t)header.fieldDefaultValuesSize <= buf.size()) {
        auto fv = readArray<Il2CppFieldDefaultValue>(
            header.fieldDefaultValuesOffset, header.fieldDefaultValuesSize,
            Il2CppFieldDefaultValue::structSize(),
            [](BinaryStream& s){ return Il2CppFieldDefaultValue::read(s); });
        for (auto& f : fv) fieldDefaultValuesDic[f.fieldIndex] = f;
    }

    {
        auto pv = readArray<Il2CppParameterDefaultValue>(
            header.parameterDefaultValuesOffset, header.parameterDefaultValuesSize,
            Il2CppParameterDefaultValue::structSize(),
            [](BinaryStream& s){ return Il2CppParameterDefaultValue::read(s); });
        for (auto& p : pv) parameterDefaultValuesDic[p.parameterIndex] = p;
    }

    propertyDefs = readArray<Il2CppPropertyDefinition>(
        header.propertiesOffset, header.propertiesSize,
        Il2CppPropertyDefinition::structSize(version),
        [&](BinaryStream& s){ return Il2CppPropertyDefinition::read(s, version); });

    if (header.interfacesSize > 0) {
        size_t cnt = (size_t)header.interfacesSize / 4;
        seek(header.interfacesOffset);
        interfaceIndices.resize(cnt);
        for (size_t i = 0; i < cnt; i++) interfaceIndices[i] = ri32();
    }

    if (header.nestedTypesSize > 0) {
        size_t cnt = (size_t)header.nestedTypesSize / 4;
        seek(header.nestedTypesOffset);
        nestedTypeIndices.resize(cnt);
        for (size_t i = 0; i < cnt; i++) nestedTypeIndices[i] = ri32();
    }

    eventDefs = readArray<Il2CppEventDefinition>(
        header.eventsOffset, header.eventsSize,
        Il2CppEventDefinition::structSize(version),
        [&](BinaryStream& s){ return Il2CppEventDefinition::read(s, version); });

    genericContainers = readArray<Il2CppGenericContainer>(
        header.genericContainersOffset, header.genericContainersSize,
        Il2CppGenericContainer::structSize(),
        [](BinaryStream& s){ return Il2CppGenericContainer::read(s); });

    genericParameters = readArray<Il2CppGenericParameter>(
        header.genericParametersOffset, header.genericParametersSize,
        Il2CppGenericParameter::structSize(),
        [](BinaryStream& s){ return Il2CppGenericParameter::read(s); });

    if (header.genericParameterConstraintsSize > 0) {
        size_t cnt = (size_t)header.genericParameterConstraintsSize / 4;
        seek(header.genericParameterConstraintsOffset);
        constraintIndices.resize(cnt);
        for (size_t i = 0; i < cnt; i++) constraintIndices[i] = ri32();
    }

    if (header.vtableMethodsSize > 0) {
        size_t cnt = (size_t)header.vtableMethodsSize / 4;
        seek(header.vtableMethodsOffset);
        vtableMethods.resize(cnt);
        for (size_t i = 0; i < cnt; i++) vtableMethods[i] = ru32();
    }

    stringLiterals = readArray<Il2CppStringLiteral>(
        header.stringLiteralOffset, header.stringLiteralSize,
        Il2CppStringLiteral::structSize(),
        [](BinaryStream& s){ return Il2CppStringLiteral::read(s); });

    fieldRefs = readArray<Il2CppFieldRef>(
        header.fieldRefsOffset, header.fieldRefsSize,
        Il2CppFieldRef::structSize(),
        [](BinaryStream& s){ return Il2CppFieldRef::read(s); });

    if (version < 27.0) {
        usageLists = readArray<Il2CppMetadataUsageList>(
            header.metadataUsageListsOffset, header.metadataUsageListsCount,
            Il2CppMetadataUsageList::structSize(),
            [](BinaryStream& s){ return Il2CppMetadataUsageList::read(s); });
        usagePairs = readArray<Il2CppMetadataUsagePair>(
            header.metadataUsagePairsOffset, header.metadataUsagePairsCount,
            Il2CppMetadataUsagePair::structSize(),
            [](BinaryStream& s){ return Il2CppMetadataUsagePair::read(s); });
        processingMetadataUsage();
    }

    if (version >= 21.0 && version < 29.0) {
        attributeTypeRanges = readArray<Il2CppCustomAttributeTypeRange>(
            header.attributesInfoOffset, header.attributesInfoCount,
            Il2CppCustomAttributeTypeRange::structSize(version),
            [&](BinaryStream& s){ return Il2CppCustomAttributeTypeRange::read(s, version); });
        if (header.attributeTypesCount > 0) {
            size_t cnt = (size_t)header.attributeTypesCount / 4;
            seek(header.attributeTypesOffset);
            attributeTypes.resize(cnt);
            for (size_t i = 0; i < cnt; i++) attributeTypes[i] = ri32();
        }
    }

    if (version >= 29.0) {
        attributeDataRanges = readArray<Il2CppCustomAttributeDataRange>(
            header.attributeDataRangeOffset, header.attributeDataRangeSize,
            Il2CppCustomAttributeDataRange::structSize(),
            [](BinaryStream& s){ return Il2CppCustomAttributeDataRange::read(s); });
    }

    if (version <= 24.1) {
        double saveVer = version; version = 16.0;
        rgctxEntries = readArray<Il2CppRGCTXDefinition>(
            header.rgctxEntriesOffset, header.rgctxEntriesCount,
            Il2CppRGCTXDefinition::structSize(version),
            [&](BinaryStream& s){ return Il2CppRGCTXDefinition::read(s, version); });
        version = saveVer;
    }
}

void Metadata::processingMetadataUsage() {
    for (uint32_t i = 1; i <= 6; i++)
        metadataUsageDic[(Il2CppMetadataUsage)i] = {};

    for (auto& lst : usageLists) {
        for (uint32_t i = 0; i < lst.count; i++) {
            uint32_t offset = lst.start + i;
            if (offset >= usagePairs.size()) continue;
            auto& pair = usagePairs[offset];
            uint32_t usage = getEncodedIndexType(pair.encodedSourceIndex);
            uint32_t decoded = getDecodedMethodIndex(pair.encodedSourceIndex);
            if (usage >= 1 && usage <= 6)
                metadataUsageDic[(Il2CppMetadataUsage)usage][pair.destinationIndex] = decoded;
        }
    }

    uint32_t maxIdx = 0;
    for (auto& kv : metadataUsageDic)
        for (auto& kv2 : kv.second)
            if (kv2.first > maxIdx) maxIdx = kv2.first;
    metadataUsagesCount = (int64_t)maxIdx + 1;
}

const std::string& Metadata::getStringFromIndex(uint32_t index) {
    auto it = stringCache.find(index);
    if (it != stringCache.end()) return it->second;
    auto s = readStringToNull(header.stringOffset + index);
    return stringCache.emplace(index, std::move(s)).first->second;
}

std::string Metadata::getStringLiteralFromIndex(uint32_t index) {
    auto& sl = stringLiterals[index];
    seek(header.stringLiteralDataOffset + sl.dataIndex);
    auto bytes = readBytes((size_t)sl.length);
    return std::string(bytes.begin(), bytes.end());
}

int32_t Metadata::getCustomAttributeIndex(const Il2CppImageDefinition& imageDef,
                                          int32_t customAttributeIndex, uint32_t token) const {
    (void)imageDef;
    (void)token;
    return customAttributeIndex;
}
