#pragma once
#include "Metadata.h"
#include "Il2CppBase.h"
#include "Il2CppConstants.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <variant>
#include <optional>

struct BlobValue {
    Il2CppTypeEnum il2CppTypeEnum = Il2CppTypeEnum::IL2CPP_TYPE_END;
    Il2CppType* EnumType = nullptr;
    std::variant<std::monostate,
        bool, uint8_t, int8_t, char16_t, uint16_t, int16_t,
        uint32_t, int32_t, uint64_t, int64_t, float, double,
        std::string, std::nullptr_t, std::vector<BlobValue>,
        Il2CppType*> value;
    bool isNull = false;
};

class Il2CppExecutor {
public:
    Metadata*   metadata;
    Il2CppBase* il2Cpp;
    std::vector<uint64_t> customAttributeGenerators;

    Il2CppExecutor(Metadata* meta, Il2CppBase* il) : metadata(meta), il2Cpp(il) {
        customAttributeGenerators = il->customAttributeGenerators;
    }

    std::string getTypeName(const Il2CppType& t, bool addNamespace, bool isNested);
    std::string getTypeDefName(const Il2CppTypeDefinition& typeDef, bool addNamespace, bool isNested);
    std::string getGenericContainerParams(const Il2CppGenericContainer& gc);
    std::string getGenericInstParams(const Il2CppGenericInst& gi);
    std::pair<std::string,std::string> getMethodSpecName(const Il2CppMethodSpec& spec);
    Il2CppTypeDefinition* getGenericClassTypeDefinition(const Il2CppGenericClass& gc);
    Il2CppTypeDefinition* getTypeDefinitionFromIl2CppType(const Il2CppType& t);
    Il2CppGenericParameter* getGenericParameterFromIl2CppType(const Il2CppType& t);
    bool tryGetDefaultValue(int typeIndex, int dataIndex, BlobValue& out);
    bool getConstantValueFromBlob(Il2CppTypeEnum type, BinaryStream& reader, BlobValue& out);
    Il2CppTypeEnum readEncodedTypeEnum(BinaryStream& reader, Il2CppType** enumTypeOut);

private:
    static const std::unordered_map<int,std::string> TypeString;
};
