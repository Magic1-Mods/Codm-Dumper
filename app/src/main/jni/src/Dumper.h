#pragma once
#include "Il2CppExecutor.h"
#include <string>
#include <unordered_map>

struct DumpConfig {
    bool DumpMethod       = true;
    bool DumpField        = true;
    bool DumpProperty     = true;
    bool DumpAttribute    = true;
    bool DumpFieldOffset  = true;
    bool DumpMethodOffset = true;
    bool DumpTypeDefIndex = true;
};

class Dumper {
public:
    Il2CppExecutor* executor;
    Metadata*       metadata;
    Il2CppBase*     il2Cpp;

    explicit Dumper(Il2CppExecutor* ex)
        : executor(ex), metadata(ex->metadata), il2Cpp(ex->il2Cpp) {}

    void dump(const DumpConfig& cfg, const std::string& outputDir);
    std::string getCustomAttribute(const Il2CppImageDefinition& imageDef,
                                   int32_t customAttributeIndex, uint32_t token,
                                   const std::string& padding = "");
    std::string getModifiers(const Il2CppMethodDefinition& methodDef);

private:
    std::unordered_map<size_t, std::string> methodModifiersCache;
};
