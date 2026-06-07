#include "Il2CppExecutor.h"
#include <sstream>

static thread_local int gTypeNameDepth = 0;

const std::unordered_map<int,std::string> Il2CppExecutor::TypeString = {
    {1,"void"},{2,"bool"},{3,"char"},{4,"sbyte"},{5,"byte"},
    {6,"short"},{7,"ushort"},{8,"int"},{9,"uint"},{10,"long"},
    {11,"ulong"},{12,"float"},{13,"double"},{14,"string"},
    {22,"TypedReference"},{24,"IntPtr"},{25,"UIntPtr"},{28,"object"},
};

Il2CppTypeDefinition* Il2CppExecutor::getGenericClassTypeDefinition(const Il2CppGenericClass& gc) {
    if (il2Cpp->version >= 27.0) {
        auto* t = il2Cpp->getIl2CppType(gc.type);
        if (!t) return nullptr;
        return getTypeDefinitionFromIl2CppType(*t);
    }
    if (gc.typeDefinitionIndex == (int64_t)(int32_t)0xFFFFFFFF || gc.typeDefinitionIndex == -1)
        return nullptr;
    if ((size_t)gc.typeDefinitionIndex >= metadata->typeDefs.size()) return nullptr;
    return &metadata->typeDefs[(size_t)gc.typeDefinitionIndex];
}

Il2CppTypeDefinition* Il2CppExecutor::getTypeDefinitionFromIl2CppType(const Il2CppType& t) {
    // 64-bit runtime stores typeHandle pointer (v22+); convert to TypeDefIndex
    if (!il2Cpp->is32bit && il2Cpp->version >= 22.0 && metadata->ImageBase != 0) {
        if (t.data < metadata->ImageBase + metadata->header.typeDefinitionsOffset) return nullptr;
        uint64_t offset = t.data - metadata->ImageBase - metadata->header.typeDefinitionsOffset;
        size_t sz = Il2CppTypeDefinition::structSize(il2Cpp->version);
        if (sz == 0) return nullptr;
        size_t idx = (size_t)(offset / sz);
        if (idx >= metadata->typeDefs.size()) return nullptr;
        return &metadata->typeDefs[idx];
    }
    int64_t idx = (int64_t)(int32_t)t.data;
    if (idx < 0 || (size_t)idx >= metadata->typeDefs.size()) return nullptr;
    return &metadata->typeDefs[(size_t)idx];
}

Il2CppGenericParameter* Il2CppExecutor::getGenericParameterFromIl2CppType(const Il2CppType& t) {
    // 64-bit runtime stores genericParameterHandle pointer (v22+)
    if (!il2Cpp->is32bit && il2Cpp->version >= 22.0 && metadata->ImageBase != 0) {
        if (t.data < metadata->ImageBase + metadata->header.genericParametersOffset) return nullptr;
        uint64_t offset = t.data - metadata->ImageBase - metadata->header.genericParametersOffset;
        size_t sz = Il2CppGenericParameter::structSize();
        if (sz == 0) return nullptr;
        size_t idx = (size_t)(offset / sz);
        if (idx >= metadata->genericParameters.size()) return nullptr;
        return &metadata->genericParameters[idx];
    }
    int64_t idx = (int64_t)(int32_t)t.data;
    if (idx < 0 || (size_t)idx >= metadata->genericParameters.size()) return nullptr;
    return &metadata->genericParameters[(size_t)idx];
}

std::string Il2CppExecutor::getTypeName(const Il2CppType& t, bool addNamespace, bool isNested) {
    if (gTypeNameDepth > 16) return "object";
    gTypeNameDepth++;
    struct DepthGuard { ~DepthGuard() { gTypeNameDepth--; } } guard;

    try {
        switch (t.type) {
            case Il2CppTypeEnum::IL2CPP_TYPE_ARRAY: {
                il2Cpp->seek(il2Cpp->mapVATR(t.data));
                auto arr = Il2CppArrayType::read(*il2Cpp);
                auto* et = il2Cpp->getIl2CppType(arr.etype);
                if (!et) return "object";
                std::string base = getTypeName(*et, addNamespace, false);
                base += "[";
                for (int i = 0; i < arr.rank-1; i++) base += ",";
                base += "]";
                return base;
            }
            case Il2CppTypeEnum::IL2CPP_TYPE_SZARRAY: {
                auto* et = il2Cpp->getIl2CppType(t.data);
                return et ? getTypeName(*et, addNamespace, false) + "[]" : "object[]";
            }
            case Il2CppTypeEnum::IL2CPP_TYPE_PTR: {
                auto* ot = il2Cpp->getIl2CppType(t.data);
                return ot ? getTypeName(*ot, addNamespace, false) + "*" : "void*";
            }
            case Il2CppTypeEnum::IL2CPP_TYPE_VAR:
            case Il2CppTypeEnum::IL2CPP_TYPE_MVAR: {
                auto* gp = getGenericParameterFromIl2CppType(t);
                if (!gp) return "T";
                return metadata->getStringFromIndex(gp->nameIndex);
            }
            case Il2CppTypeEnum::IL2CPP_TYPE_CLASS:
            case Il2CppTypeEnum::IL2CPP_TYPE_VALUETYPE:
            case Il2CppTypeEnum::IL2CPP_TYPE_GENERICINST: {
                Il2CppTypeDefinition* typeDef = nullptr;
                std::string gcStr;
                if (t.type == Il2CppTypeEnum::IL2CPP_TYPE_GENERICINST) {
                    try {
                        il2Cpp->seek(il2Cpp->mapVATR(t.data));
                        auto gcVal = Il2CppGenericClass::read(*il2Cpp, il2Cpp->version);
                        typeDef = getGenericClassTypeDefinition(gcVal);
                        if (!typeDef) return "object";
                        if (gcVal.context.class_inst) {
                            try {
                                il2Cpp->seek(il2Cpp->mapVATR(gcVal.context.class_inst));
                                auto gi = Il2CppGenericInst::read(*il2Cpp);
                                gcStr = getGenericInstParams(gi);
                            } catch (...) {}
                        }
                    } catch (...) { return "object"; }
                } else {
                    typeDef = getTypeDefinitionFromIl2CppType(t);
                    if (!typeDef) return "object";
                }
                std::string result;
                if (addNamespace) {
                    auto& ns = metadata->getStringFromIndex(typeDef->namespaceIndex);
                    if (!ns.empty()) result = ns + ".";
                }
                result += metadata->getStringFromIndex(typeDef->nameIndex);
                if (typeDef->genericContainerIndex >= 0 && gcStr.empty()) {
                    if ((size_t)typeDef->genericContainerIndex < metadata->genericContainers.size()) {
                        auto& gc2 = metadata->genericContainers[(size_t)typeDef->genericContainerIndex];
                        result += getGenericContainerParams(gc2);
                    }
                } else if (!gcStr.empty()) {
                    auto pos = result.find('`');
                    if (pos != std::string::npos) result = result.substr(0, pos);
                    result += "<" + gcStr + ">";
                }
                return result;
            }
            default: {
                auto it = TypeString.find((int)t.type);
                if (it != TypeString.end()) return it->second;
                return "object";
            }
        }
    } catch (...) { return "object"; }
}

static thread_local int gTypeDefNameDepth = 0;

std::string Il2CppExecutor::getTypeDefName(const Il2CppTypeDefinition& typeDef, bool addNamespace, bool isNested) {
    if (gTypeDefNameDepth > 8) return "Unknown";
    gTypeDefNameDepth++;
    struct Guard { ~Guard() { gTypeDefNameDepth--; } } g;
    try {
        std::string result;
        if (typeDef.declaringTypeIndex >= 0 &&
            (size_t)typeDef.declaringTypeIndex < il2Cpp->types.size()) {
            auto* declaringType = getTypeDefinitionFromIl2CppType(il2Cpp->types[(size_t)typeDef.declaringTypeIndex]);
            if (declaringType) {
                result = getTypeDefName(*declaringType, addNamespace, true) + ".";
            }
        } else if (addNamespace) {
            auto& ns = metadata->getStringFromIndex(typeDef.namespaceIndex);
            if (!ns.empty()) result = ns + ".";
        }
        result += metadata->getStringFromIndex(typeDef.nameIndex);
        if (typeDef.genericContainerIndex >= 0 &&
            (size_t)typeDef.genericContainerIndex < metadata->genericContainers.size()) {
            auto& gc = metadata->genericContainers[(size_t)typeDef.genericContainerIndex];
            result += getGenericContainerParams(gc);
        }
        return result;
    } catch (...) { return "Unknown"; }
}

std::string Il2CppExecutor::getGenericContainerParams(const Il2CppGenericContainer& gc) {
    if (gc.type_argc <= 0 || gc.type_argc > 64) return "";
    try {
        std::string s = "<";
        for (int i = 0; i < gc.type_argc; i++) {
            if (i > 0) s += ", ";
            int pIdx = gc.genericParameterStart + i;
            if (pIdx >= 0 && (size_t)pIdx < metadata->genericParameters.size())
                s += metadata->getStringFromIndex(metadata->genericParameters[(size_t)pIdx].nameIndex);
            else
                s += "T" + std::to_string(i);
        }
        s += ">";
        return s;
    } catch (...) { return ""; }
}

std::string Il2CppExecutor::getGenericInstParams(const Il2CppGenericInst& gi) {
    if (gi.type_argc <= 0 || gi.type_argc > 64) return "";
    try {
        auto typeArgPtrs = il2Cpp->mapVATR_ptrs(gi.type_argv, gi.type_argc);
        std::string s;
        for (int64_t i = 0; i < gi.type_argc; i++) {
            if (i > 0) s += ", ";
            auto* t = il2Cpp->getIl2CppType(typeArgPtrs[(size_t)i]);
            s += t ? getTypeName(*t, false, false) : "object";
        }
        return s;
    } catch (...) { return ""; }
}

std::pair<std::string,std::string> Il2CppExecutor::getMethodSpecName(const Il2CppMethodSpec& spec) {
    try {
        if (spec.methodDefinitionIndex < 0 ||
            (size_t)spec.methodDefinitionIndex >= metadata->methodDefs.size())
            return {"Unknown", "Unknown"};
        auto& methodDef = metadata->methodDefs[(size_t)spec.methodDefinitionIndex];
        if (methodDef.declaringType < 0 ||
            (size_t)methodDef.declaringType >= metadata->typeDefs.size())
            return {"Unknown", "Unknown"};
        auto& typeDef = metadata->typeDefs[(size_t)methodDef.declaringType];
        std::string typeName = getTypeDefName(typeDef, false, false);
        if (spec.classIndexIndex >= 0 && (size_t)spec.classIndexIndex < il2Cpp->genericInsts.size()) {
            auto& gi = il2Cpp->genericInsts[(size_t)spec.classIndexIndex];
            auto pos = typeName.find('`');
            if (pos != std::string::npos) typeName = typeName.substr(0, pos);
            typeName += "<" + getGenericInstParams(gi) + ">";
        }
        std::string methodName = metadata->getStringFromIndex(methodDef.nameIndex);
        if (spec.methodIndexIndex >= 0 && (size_t)spec.methodIndexIndex < il2Cpp->genericInsts.size()) {
            auto& gi = il2Cpp->genericInsts[(size_t)spec.methodIndexIndex];
            methodName += "<" + getGenericInstParams(gi) + ">";
        }
        return {typeName, methodName};
    } catch (...) { return {"Unknown", "Unknown"}; }
}

bool Il2CppExecutor::tryGetDefaultValue(int typeIndex, int dataIndex, BlobValue& out) {
    try {
        if (typeIndex < 0 || (size_t)typeIndex >= il2Cpp->types.size()) return false;
        uint32_t pointer = metadata->getDefaultValueFromIndex(dataIndex);
        auto& defType = il2Cpp->types[(size_t)typeIndex];
        metadata->seek(pointer);
        return getConstantValueFromBlob(defType.type, *metadata, out);
    } catch (...) { return false; }
}

Il2CppTypeEnum Il2CppExecutor::readEncodedTypeEnum(BinaryStream& reader, Il2CppType** enumTypeOut) {
    if (enumTypeOut) *enumTypeOut = nullptr;
    uint8_t b = reader.ru8();
    auto type = (Il2CppTypeEnum)b;
    if (type == Il2CppTypeEnum::IL2CPP_TYPE_ENUM) {
        int32_t eti = reader.readCompressedI32();
        if (eti >= 0 && (size_t)eti < il2Cpp->types.size()) {
            if (enumTypeOut) *enumTypeOut = &il2Cpp->types[(size_t)eti];
            auto* td = getTypeDefinitionFromIl2CppType(il2Cpp->types[(size_t)eti]);
            if (td && (size_t)td->elementTypeIndex < il2Cpp->types.size())
                type = il2Cpp->types[(size_t)td->elementTypeIndex].type;
        }
    }
    return type;
}

bool Il2CppExecutor::getConstantValueFromBlob(Il2CppTypeEnum type, BinaryStream& reader, BlobValue& out) {
    out.il2CppTypeEnum = type;
    try {
        switch (type) {
            case Il2CppTypeEnum::IL2CPP_TYPE_BOOLEAN: out.value = reader.ru8() != 0; return true;
            case Il2CppTypeEnum::IL2CPP_TYPE_U1:      out.value = reader.ru8();      return true;
            case Il2CppTypeEnum::IL2CPP_TYPE_I1:      out.value = reader.ri8();      return true;
            case Il2CppTypeEnum::IL2CPP_TYPE_CHAR: {
                uint8_t b[2]; b[0]=reader.ru8(); b[1]=reader.ru8();
                char16_t c; memcpy(&c, b, 2); out.value = c; return true;
            }
            case Il2CppTypeEnum::IL2CPP_TYPE_U2: out.value = reader.ru16(); return true;
            case Il2CppTypeEnum::IL2CPP_TYPE_I2: out.value = reader.ri16(); return true;
            case Il2CppTypeEnum::IL2CPP_TYPE_U4:
                out.value = (il2Cpp->version >= 29.0) ? reader.readCompressedU32() : reader.ru32();
                return true;
            case Il2CppTypeEnum::IL2CPP_TYPE_I4:
                out.value = (il2Cpp->version >= 29.0) ? reader.readCompressedI32() : reader.ri32();
                return true;
            case Il2CppTypeEnum::IL2CPP_TYPE_U8: out.value = reader.ru64(); return true;
            case Il2CppTypeEnum::IL2CPP_TYPE_I8: out.value = reader.ri64(); return true;
            case Il2CppTypeEnum::IL2CPP_TYPE_R4: { float f; uint32_t v=reader.ru32(); memcpy(&f,&v,4); out.value=f; return true; }
            case Il2CppTypeEnum::IL2CPP_TYPE_R8: { double d; uint64_t v=reader.ru64(); memcpy(&d,&v,8); out.value=d; return true; }
            case Il2CppTypeEnum::IL2CPP_TYPE_STRING: {
                if (il2Cpp->version >= 29.0) {
                    int32_t len = reader.readCompressedI32();
                    if (len == -1) { out.isNull=true; out.value=std::string(); return true; }
                    if (len < 0 || len > 65536) return false;
                    auto bytes = reader.readBytes((size_t)len);
                    out.value = std::string(bytes.begin(), bytes.end());
                } else {
                    int32_t len = reader.ri32();
                    if (len < 0 || len > 65536) return false;
                    auto bytes = reader.readBytes((size_t)len);
                    out.value = std::string(bytes.begin(), bytes.end());
                }
                return true;
            }
            case Il2CppTypeEnum::IL2CPP_TYPE_SZARRAY: {
                int32_t arrLen = reader.readCompressedI32();
                if (arrLen == -1) { out.isNull=true; out.value=std::vector<BlobValue>(); return true; }
                if (arrLen < 0 || arrLen > 4096) return false;
                Il2CppType* enumType = nullptr;
                auto elemType = readEncodedTypeEnum(reader, &enumType);
                uint8_t elemsDiffer = reader.ru8();
                std::vector<BlobValue> arr;
                for (int32_t i = 0; i < arrLen; i++) {
                    auto curType = elemType;
                    if (elemsDiffer == 1) {
                        Il2CppType* et2 = nullptr;
                        curType = readEncodedTypeEnum(reader, &et2);
                    }
                    BlobValue elem;
                    getConstantValueFromBlob(curType, reader, elem);
                    arr.push_back(std::move(elem));
                }
                out.value = std::move(arr);
                return true;
            }
            case Il2CppTypeEnum::IL2CPP_TYPE_IL2CPP_TYPE_INDEX: {
                int32_t ti = reader.readCompressedI32();
                if (ti == -1) { out.isNull=true; return true; }
                if ((size_t)ti < il2Cpp->types.size()) out.value = &il2Cpp->types[(size_t)ti];
                return true;
            }
            default: return false;
        }
    } catch (...) { return false; }
}
