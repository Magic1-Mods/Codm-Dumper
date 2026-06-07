#include "Il2CppBase.h"
#include "SectionHelper.h"
#include <cstdio>
#include <algorithm>

bool Il2CppBase::autoPlusInit(uint64_t codeReg, uint64_t metaReg) {
    if (codeReg != 0) {
        double limit = 327680.0;
        if (version >= 24.2) {
            try {
                seek(mapVATR(codeReg));
                pCodeReg = Il2CppCodeRegistration::read(*this, version);
            } catch (...) { goto printAndInit; }

            if (version == 31.0) {
                if (pCodeReg.genericMethodPointersCount > (uint64_t)limit)
                    codeReg -= ptrSize() * 2;
                else { version = 29.0; printf("Change il2cpp version to: %.1f\n", version); }
            }
            if (version == 29.0) {
                if (pCodeReg.genericMethodPointersCount > (uint64_t)limit) {
                    version = 29.1; codeReg -= ptrSize() * 2;
                    printf("Change il2cpp version to: %.1f\n", version);
                }
            }
            if (version == 27.0) {
                if (pCodeReg.reversePInvokeWrapperCount > (uint64_t)limit) {
                    version = 27.1; codeReg -= ptrSize();
                    printf("Change il2cpp version to: %.1f\n", version);
                }
            }
            if (version == 24.4) {
                codeReg -= ptrSize() * 2;
                try { seek(mapVATR(codeReg)); pCodeReg = Il2CppCodeRegistration::read(*this, version); } catch (...) {}
                if (pCodeReg.reversePInvokeWrapperCount > (uint64_t)limit) {
                    version = 24.5; codeReg -= ptrSize();
                    printf("Change il2cpp version to: %.1f\n", version);
                }
            }
            if (version == 24.2) {
                if (pCodeReg.interopDataCount == 0) {
                    version = 24.3; codeReg -= ptrSize() * 2;
                    printf("Change il2cpp version to: %.1f\n", version);
                }
            }
        }
    }
printAndInit:
    printf("CodeRegistration : %llx\n", (unsigned long long)codeReg);
    printf("MetadataRegistration : %llx\n", (unsigned long long)metaReg);
    if (codeReg != 0 && metaReg != 0) {
        init(codeReg, metaReg);
        return true;
    }
    return false;
}

void Il2CppBase::init(uint64_t codeReg, uint64_t metaReg) {
    try { seek(mapVATR(codeReg)); pCodeReg = Il2CppCodeRegistration::read(*this, version); }
    catch (...) { printf("ERROR: Failed to read CodeRegistration\n"); return; }

    double limit = 327680.0;
    if (version == 27.0 && pCodeReg.invokerPointersCount > (uint64_t)limit) {
        version = 27.1; printf("Change il2cpp version to: %.1f\n", version);
        try { seek(mapVATR(codeReg)); pCodeReg = Il2CppCodeRegistration::read(*this, version); } catch (...) {}
    }
    if (version == 27.1) {
        try {
            auto pCodeGenModulePtrs = mapVATR_ptrs(pCodeReg.codeGenModules, pCodeReg.codeGenModulesCount);
            for (auto ptr : pCodeGenModulePtrs) {
                try {
                    seek(mapVATR(ptr));
                    auto cgm = Il2CppCodeGenModule::read(*this, version);
                    if (cgm.rgctxsCount > 0) {
                        auto rgctxs = mapVATR_array<Il2CppRGCTXDefinition>(cgm.rgctxs, cgm.rgctxsCount);
                        bool allBig = true;
                        for (auto& r : rgctxs)
                            if ((uint64_t)r.data_pre29 <= (uint64_t)limit) { allBig=false; break; }
                        if (allBig) { version = 27.2; printf("Change il2cpp version to: %.1f\n", version); }
                        break;
                    }
                } catch (...) {}
            }
        } catch (...) {}
    }
    if (version == 24.4 && pCodeReg.invokerPointersCount > (uint64_t)limit) {
        version = 24.5; printf("Change il2cpp version to: %.1f\n", version);
        try { seek(mapVATR(codeReg)); pCodeReg = Il2CppCodeRegistration::read(*this, version); } catch (...) {}
    }
    if (version == 24.2 && pCodeReg.codeGenModules == 0) {
        version = 24.3; printf("Change il2cpp version to: %.1f\n", version);
        try { seek(mapVATR(codeReg)); pCodeReg = Il2CppCodeRegistration::read(*this, version); } catch (...) {}
    }

    try { seek(mapVATR(metaReg)); pMetaReg = Il2CppMetadataRegistration::read(*this, version); }
    catch (...) { printf("ERROR: Failed to read MetadataRegistration\n"); return; }

    try { genericMethodPointers = mapVATR_ptrs(pCodeReg.genericMethodPointers, (int64_t)pCodeReg.genericMethodPointersCount); } catch (...) {}
    try { invokerPointers        = mapVATR_ptrs(pCodeReg.invokerPointers,       (int64_t)pCodeReg.invokerPointersCount); }        catch (...) {}

    if (version < 27.0) {
        try { customAttributeGenerators = mapVATR_ptrs(pCodeReg.customAttributeGenerators, (int64_t)pCodeReg.customAttributeCount); } catch (...) {}
    }
    if (version > 16.0 && version < 27.0) {
        try { metadataUsages = mapVATR_ptrs(pMetaReg.metadataUsages, metadataUsagesCount); } catch (...) {}
    }
    if (version >= 22.0) {
        if (pCodeReg.reversePInvokeWrapperCount != 0)
            try { reversePInvokeWrappers = mapVATR_ptrs(pCodeReg.reversePInvokeWrappers, (int64_t)pCodeReg.reversePInvokeWrapperCount); } catch (...) {}
        if (pCodeReg.unresolvedVirtualCallCount != 0)
            try { unresolvedVirtualCallPointers = mapVATR_ptrs(pCodeReg.unresolvedVirtualCallPointers, (int64_t)pCodeReg.unresolvedVirtualCallCount); } catch (...) {}
    }

    try {
        genericInstPointers = mapVATR_ptrs(pMetaReg.genericInsts, pMetaReg.genericInstsCount);
        genericInsts.reserve(genericInstPointers.size());
        for (auto p : genericInstPointers) {
            try { seek(mapVATR(p)); genericInsts.push_back(Il2CppGenericInst::read(*this)); }
            catch (...) { genericInsts.push_back({}); }
        }
    } catch (...) {}

    fieldOffsetsArePointers = (version > 21.0);
    if (version == 21.0) {
        try {
            seek(mapVATR(pMetaReg.fieldOffsets));
            uint32_t test[6];
            for (int i = 0; i < 6; i++) test[i] = ru32();
            fieldOffsetsArePointers = (test[0]==0&&test[1]==0&&test[2]==0&&test[3]==0&&test[4]==0&&test[5]>0);
        } catch (...) {}
    }

    if (fieldOffsetsArePointers) {
        try { fieldOffsets = mapVATR_ptrs(pMetaReg.fieldOffsets, pMetaReg.fieldOffsetsCount); } catch (...) {}
    } else {
        try {
            seek(mapVATR(pMetaReg.fieldOffsets));
            fieldOffsets.resize((size_t)pMetaReg.fieldOffsetsCount);
            for (int64_t i = 0; i < pMetaReg.fieldOffsetsCount; i++)
                fieldOffsets[i] = (uint64_t)ru32();
        } catch (...) {}
    }

    try {
        auto pTypes = mapVATR_ptrs(pMetaReg.types, pMetaReg.typesCount);
        types.reserve((size_t)pMetaReg.typesCount);
        for (size_t i = 0; i < (size_t)pMetaReg.typesCount; i++) {
            try {
                // Ported from newer C# fork: normalize pointer before mapVATR
                uint64_t typePtr = normalizePointer(pTypes[i]);
                seek(mapVATR(typePtr));
                Il2CppType t;
                t.data = ruptr();
                t.bits = ru32();
                t.init(version);
                typeDic[typePtr] = types.size();
                types.push_back(t);
            } catch (...) { types.push_back({}); }
        }
    } catch (...) {}

    if (version >= 24.2) {
        try {
            auto pCgmPtrs = mapVATR_ptrs(pCodeReg.codeGenModules, (int64_t)pCodeReg.codeGenModulesCount);
            for (auto ptr : pCgmPtrs) {
                try {
                    seek(mapVATR(ptr));
                    auto cgm = Il2CppCodeGenModule::read(*this, version);
                    std::string modName;
                    try { modName = mapVATR_string(cgm.moduleName); } catch (...) { modName = "unknown"; }
                    codeGenModules[modName] = cgm;

                    std::vector<uint64_t> mptrs;
                    try { mptrs = mapVATR_ptrs(cgm.methodPointers, cgm.methodPointerCount); }
                    catch (...) { mptrs.resize((size_t)cgm.methodPointerCount, 0); }
                    codeGenModuleMethodPointers[modName] = mptrs;

                    std::unordered_map<uint32_t, std::vector<Il2CppRGCTXDefinition>> rgctxDef;
                    if (cgm.rgctxsCount > 0) {
                        try {
                            auto rgctxs      = mapVATR_array<Il2CppRGCTXDefinition>(cgm.rgctxs, cgm.rgctxsCount);
                            auto rgctxRanges = mapVATR_array<Il2CppTokenRangePair>(cgm.rgctxRanges, cgm.rgctxRangesCount);
                            for (auto& rr : rgctxRanges) {
                                if (rr.range.start < 0 || rr.range.length <= 0) continue;
                                size_t start = (size_t)rr.range.start;
                                size_t len   = (size_t)rr.range.length;
                                if (start + len > rgctxs.size()) continue;
                                rgctxDef[rr.token] = std::vector<Il2CppRGCTXDefinition>(
                                    rgctxs.begin()+start, rgctxs.begin()+start+len);
                            }
                        } catch (...) {}
                    }
                    rgctxsDictionary[modName] = std::move(rgctxDef);
                } catch (...) {}
            }
        } catch (...) {}
    } else {
        try { methodPointers = mapVATR_ptrs(pCodeReg.methodPointers, (int64_t)pCodeReg.methodPointersCount); } catch (...) {}
    }

    try {
        seek(mapVATR(pMetaReg.genericMethodTable));
        std::vector<Il2CppGenericMethodFunctionsDefinitions> genericMethodTable;
        genericMethodTable.reserve((size_t)pMetaReg.genericMethodTableCount);
        for (int64_t i = 0; i < pMetaReg.genericMethodTableCount; i++)
            genericMethodTable.push_back(Il2CppGenericMethodFunctionsDefinitions::read(*this, version));

        seek(mapVATR(pMetaReg.methodSpecs));
        methodSpecs.reserve((size_t)pMetaReg.methodSpecsCount);
        for (int64_t i = 0; i < pMetaReg.methodSpecsCount; i++)
            methodSpecs.push_back(Il2CppMethodSpec::read(*this));

        for (auto& table : genericMethodTable) {
            try {
                if ((size_t)table.genericMethodIndex >= methodSpecs.size()) continue;
                auto& spec = methodSpecs[(size_t)table.genericMethodIndex];
                int32_t mdi = spec.methodDefinitionIndex;
                methodDefinitionMethodSpecs[mdi].push_back(spec);
                size_t specIdx = (size_t)(&spec - methodSpecs.data());
                if ((size_t)table.indices.methodIndex < genericMethodPointers.size())
                    methodSpecGenericMethodPointers[specIdx] = genericMethodPointers[(size_t)table.indices.methodIndex];
            } catch (...) {}
        }
    } catch (...) {}
}
