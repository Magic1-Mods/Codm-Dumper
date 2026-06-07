#pragma once
#include "BinaryStream.h"
#include "Il2CppStructs.h"
#include "Il2CppExecutor.h"
#include <string>
#include <vector>

class CustomAttributeReader {
public:
    uint32_t count = 0;

    CustomAttributeReader(Il2CppExecutor* ex, const std::vector<uint8_t>& buff)
        : executor(ex), metadata(ex->metadata), il2Cpp(ex->il2Cpp)
    {
        blob.buf     = buff;
        blob.version = metadata->version;
        blob.is32bit = il2Cpp->is32bit;

        count      = blob.readCompressedU32();
        ctorBuffer = blob.pos;
        dataBuffer = blob.pos + count * 4;
    }

    std::string getString() {
        if (blob.pos > blob.buf.size() || dataBuffer > blob.buf.size()) return "";
        try {
            blob.seek(ctorBuffer);
            int32_t ctorIndex = blob.ri32();
            ctorBuffer = blob.pos;

            if (ctorIndex < 0 || (size_t)ctorIndex >= metadata->methodDefs.size()) return "";
            auto& methodDef = metadata->methodDefs[(size_t)ctorIndex];
            if (methodDef.declaringType < 0 || (size_t)methodDef.declaringType >= metadata->typeDefs.size()) return "";
            auto& typeDef = metadata->typeDefs[(size_t)methodDef.declaringType];

            blob.seek(dataBuffer);
            uint32_t argCount  = blob.readCompressedU32();
            uint32_t fieldCount = blob.readCompressedU32();
            uint32_t propCount  = blob.readCompressedU32();

            if (argCount > 256 || fieldCount > 256 || propCount > 256) return "";

            std::vector<std::string> argList;

            for (uint32_t i = 0; i < argCount; i++) {
                if (blob.pos >= blob.buf.size()) break;
                argList.push_back(readDataValue());
            }
            for (uint32_t i = 0; i < fieldCount; i++) {
                if (blob.pos >= blob.buf.size()) break;
                std::string val = readDataValue();
                auto [declType, idx] = readNamedArgClassAndIndex(typeDef);
                if (idx >= 0 && (size_t)(declType.fieldStart + idx) < metadata->fieldDefs.size()) {
                    auto& fd = metadata->fieldDefs[(size_t)(declType.fieldStart + idx)];
                    std::string fn = metadata->getStringFromIndex(fd.nameIndex);
                    argList.push_back(fn + " = " + val);
                }
            }
            for (uint32_t i = 0; i < propCount; i++) {
                if (blob.pos >= blob.buf.size()) break;
                std::string val = readDataValue();
                auto [declType, idx] = readNamedArgClassAndIndex(typeDef);
                if (idx >= 0 && (size_t)(declType.propertyStart + idx) < metadata->propertyDefs.size()) {
                    auto& pd = metadata->propertyDefs[(size_t)(declType.propertyStart + idx)];
                    std::string pn = metadata->getStringFromIndex(pd.nameIndex);
                    argList.push_back(pn + " = " + val);
                }
            }
            dataBuffer = blob.pos;

            std::string typeName = metadata->getStringFromIndex(typeDef.nameIndex);
            auto pos = typeName.rfind("Attribute");
            if (pos != std::string::npos) typeName = typeName.substr(0, pos);

            if (argList.empty()) return "[" + typeName + "]";
            std::string joined;
            for (size_t i = 0; i < argList.size(); i++) {
                if (i) joined += ", ";
                joined += argList[i];
            }
            return "[" + typeName + "(" + joined + ")]";
        } catch (...) { return ""; }
    }

private:
    Il2CppExecutor* executor;
    Metadata*       metadata;
    Il2CppBase*     il2Cpp;
    BinaryStream    blob;
    size_t          ctorBuffer = 0;
    size_t          dataBuffer = 0;

    std::string blobToString(const BlobValue& bv) {
        if (bv.isNull) return "null";
        return std::visit([&](auto&& val) -> std::string {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, std::monostate>) return "";
            else if constexpr (std::is_same_v<T, bool>)   return val ? "true" : "false";
            else if constexpr (std::is_same_v<T, std::string>) {
                std::string r = "\"";
                for (char c : val) {
                    if (c=='"') r += "\\\"";
                    else if (c=='\\') r += "\\\\";
                    else r += c;
                }
                r += "\""; return r;
            }
            else if constexpr (std::is_same_v<T, Il2CppType*>) {
                if (val) return "typeof(" + executor->getTypeName(*val, false, false) + ")";
                return "null";
            }
            else if constexpr (std::is_same_v<T, std::vector<BlobValue>>) {
                std::string r = "new[] { ";
                auto& arr = val;
                for (size_t i = 0; i < arr.size(); i++) {
                    if (i) r += ", ";
                    r += blobToString(arr[i]);
                }
                r += " }"; return r;
            }
            else if constexpr (std::is_same_v<T, float>) {
                char buf[32]; snprintf(buf,sizeof(buf),"%g",(double)val); return buf;
            }
            else if constexpr (std::is_same_v<T, double>) {
                char buf[32]; snprintf(buf,sizeof(buf),"%g",val); return buf;
            }
            else if constexpr (std::is_arithmetic_v<T>) return std::to_string(val);
            return "";
        }, bv.value);
    }

    std::string readDataValue() {
        Il2CppType* enumType = nullptr;
        auto type = executor->readEncodedTypeEnum(blob, &enumType);
        BlobValue bv;
        executor->getConstantValueFromBlob(type, blob, bv);
        if (enumType) bv.EnumType = enumType;
        return blobToString(bv);
    }

    std::pair<Il2CppTypeDefinition&, int> readNamedArgClassAndIndex(Il2CppTypeDefinition& typeDef) {
        int32_t memberIndex = blob.readCompressedI32();
        if (memberIndex >= 0) return {typeDef, memberIndex};
        memberIndex = -(memberIndex + 1);
        uint32_t typeIndex = blob.readCompressedU32();
        if ((size_t)typeIndex >= metadata->typeDefs.size()) return {typeDef, memberIndex};
        return {metadata->typeDefs[(size_t)typeIndex], memberIndex};
    }
};
