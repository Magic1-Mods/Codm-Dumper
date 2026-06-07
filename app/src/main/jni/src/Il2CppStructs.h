#pragma once
#include "BinaryStream.h"
#include <cstdint>
#include <vector>

enum class Il2CppTypeEnum : uint8_t {
    IL2CPP_TYPE_END        = 0x00,
    IL2CPP_TYPE_VOID       = 0x01,
    IL2CPP_TYPE_BOOLEAN    = 0x02,
    IL2CPP_TYPE_CHAR       = 0x03,
    IL2CPP_TYPE_I1         = 0x04,
    IL2CPP_TYPE_U1         = 0x05,
    IL2CPP_TYPE_I2         = 0x06,
    IL2CPP_TYPE_U2         = 0x07,
    IL2CPP_TYPE_I4         = 0x08,
    IL2CPP_TYPE_U4         = 0x09,
    IL2CPP_TYPE_I8         = 0x0a,
    IL2CPP_TYPE_U8         = 0x0b,
    IL2CPP_TYPE_R4         = 0x0c,
    IL2CPP_TYPE_R8         = 0x0d,
    IL2CPP_TYPE_STRING     = 0x0e,
    IL2CPP_TYPE_PTR        = 0x0f,
    IL2CPP_TYPE_BYREF      = 0x10,
    IL2CPP_TYPE_VALUETYPE  = 0x11,
    IL2CPP_TYPE_CLASS      = 0x12,
    IL2CPP_TYPE_VAR        = 0x13,
    IL2CPP_TYPE_ARRAY      = 0x14,
    IL2CPP_TYPE_GENERICINST= 0x15,
    IL2CPP_TYPE_TYPEDBYREF = 0x16,
    IL2CPP_TYPE_I          = 0x18,
    IL2CPP_TYPE_U          = 0x19,
    IL2CPP_TYPE_FNPTR      = 0x1b,
    IL2CPP_TYPE_OBJECT     = 0x1c,
    IL2CPP_TYPE_SZARRAY    = 0x1d,
    IL2CPP_TYPE_MVAR       = 0x1e,
    IL2CPP_TYPE_CMOD_REQD  = 0x1f,
    IL2CPP_TYPE_CMOD_OPT   = 0x20,
    IL2CPP_TYPE_INTERNAL   = 0x21,
    IL2CPP_TYPE_MODIFIER   = 0x40,
    IL2CPP_TYPE_SENTINEL   = 0x41,
    IL2CPP_TYPE_PINNED     = 0x45,
    IL2CPP_TYPE_ENUM       = 0x55,
    IL2CPP_TYPE_IL2CPP_TYPE_INDEX = 0xff,
};

enum class Il2CppMetadataUsage : uint32_t {
    kIl2CppMetadataUsageInvalid = 0,
    kIl2CppMetadataUsageTypeInfo,
    kIl2CppMetadataUsageIl2CppType,
    kIl2CppMetadataUsageMethodDef,
    kIl2CppMetadataUsageFieldInfo,
    kIl2CppMetadataUsageStringLiteral,
    kIl2CppMetadataUsageMethodRef,
};

enum class Il2CppRGCTXDataType : int32_t {
    IL2CPP_RGCTX_DATA_INVALID = 0,
    IL2CPP_RGCTX_DATA_TYPE,
    IL2CPP_RGCTX_DATA_CLASS,
    IL2CPP_RGCTX_DATA_METHOD,
    IL2CPP_RGCTX_DATA_ARRAY,
    IL2CPP_RGCTX_DATA_CONSTRAINED,
};

// ---- Runtime / ELF structs (unchanged from Project A) ----

struct Il2CppType {
    uint64_t data = 0;
    uint32_t bits = 0;
    uint32_t attrs = 0;
    Il2CppTypeEnum type = Il2CppTypeEnum::IL2CPP_TYPE_END;
    uint32_t num_mods = 0;
    uint32_t byref = 0;
    uint32_t pinned = 0;
    uint32_t valuetype = 0;

    void init(double ver) {
        attrs = bits & 0xffff;
        type = (Il2CppTypeEnum)((bits >> 16) & 0xff);
        if (ver >= 27.2) {
            num_mods = (bits >> 24) & 0x1f;
            byref    = (bits >> 29) & 1;
            pinned   = (bits >> 30) & 1;
            valuetype = bits >> 31;
        } else {
            num_mods = (bits >> 24) & 0x3f;
            byref    = (bits >> 30) & 1;
            pinned   = bits >> 31;
        }
    }

    static Il2CppType read(BinaryStream& s, double ver) {
        Il2CppType t;
        t.data = s.ruptr();
        t.bits = s.ru32();
        t.init(ver);
        return t;
    }

    static size_t structSize(bool is32bit) {
        return is32bit ? 8 : 12;
    }
};

struct Il2CppGenericContext {
    uint64_t class_inst = 0;
    uint64_t method_inst = 0;

    static Il2CppGenericContext read(BinaryStream& s) {
        Il2CppGenericContext c;
        c.class_inst  = s.ruptr();
        c.method_inst = s.ruptr();
        return c;
    }
};

struct Il2CppGenericInst {
    int64_t  type_argc = 0;
    uint64_t type_argv = 0;

    static Il2CppGenericInst read(BinaryStream& s) {
        Il2CppGenericInst g;
        g.type_argc = s.riptr();
        g.type_argv = s.ruptr();
        return g;
    }
};

struct Il2CppArrayType {
    uint64_t etype = 0;
    uint8_t  rank = 0;
    uint8_t  numsizes = 0;
    uint8_t  numlobounds = 0;
    uint64_t sizes = 0;
    uint64_t lobounds = 0;

    static Il2CppArrayType read(BinaryStream& s) {
        Il2CppArrayType a;
        a.etype      = s.ruptr();
        a.rank       = s.ru8();
        a.numsizes   = s.ru8();
        a.numlobounds = s.ru8();
        a.sizes      = s.ruptr();
        a.lobounds   = s.ruptr();
        return a;
    }
};

struct Il2CppGenericClass {
    int64_t  typeDefinitionIndex = 0;
    uint64_t type = 0;
    Il2CppGenericContext context;
    uint64_t cached_class = 0;

    static Il2CppGenericClass read(BinaryStream& s, double ver) {
        Il2CppGenericClass g;
        if (ver <= 24.5) {
            g.typeDefinitionIndex = s.riptr();
        } else {
            g.type = s.ruptr();
        }
        g.context      = Il2CppGenericContext::read(s);
        g.cached_class = s.ruptr();
        return g;
    }
};

struct Il2CppMethodSpec {
    int32_t methodDefinitionIndex = 0;
    int32_t classIndexIndex = 0;
    int32_t methodIndexIndex = 0;

    static Il2CppMethodSpec read(BinaryStream& s) {
        Il2CppMethodSpec m;
        m.methodDefinitionIndex = s.ri32();
        m.classIndexIndex       = s.ri32();
        m.methodIndexIndex      = s.ri32();
        return m;
    }
    static size_t structSize() { return 12; }
};

struct Il2CppGenericMethodIndices {
    int32_t methodIndex = 0;
    int32_t invokerIndex = 0;
    int32_t adjustorThunk = 0;

    static Il2CppGenericMethodIndices read(BinaryStream& s, double ver) {
        Il2CppGenericMethodIndices m;
        m.methodIndex   = s.ri32();
        m.invokerIndex  = s.ri32();
        bool hasAdjustor = (ver >= 24.5 && ver <= 24.5) || ver >= 27.1;
        if (hasAdjustor) m.adjustorThunk = s.ri32();
        return m;
    }
};

struct Il2CppGenericMethodFunctionsDefinitions {
    int32_t genericMethodIndex = 0;
    Il2CppGenericMethodIndices indices;

    static Il2CppGenericMethodFunctionsDefinitions read(BinaryStream& s, double ver) {
        Il2CppGenericMethodFunctionsDefinitions d;
        d.genericMethodIndex = s.ri32();
        d.indices = Il2CppGenericMethodIndices::read(s, ver);
        return d;
    }

    static size_t structSize(double ver) {
        bool hasAdjustor = (ver >= 24.5 && ver <= 24.5) || ver >= 27.1;
        return 4 + 8 + (hasAdjustor ? 4 : 0);
    }
};

struct Il2CppRange {
    int32_t start = 0;
    int32_t length = 0;

    static Il2CppRange read(BinaryStream& s) {
        Il2CppRange r;
        r.start  = s.ri32();
        r.length = s.ri32();
        return r;
    }
};

struct Il2CppTokenRangePair {
    uint32_t token = 0;
    Il2CppRange range;

    static Il2CppTokenRangePair read(BinaryStream& s) {
        Il2CppTokenRangePair t;
        t.token = s.ru32();
        t.range = Il2CppRange::read(s);
        return t;
    }
    static size_t structSize() { return 12; }
};

struct Il2CppCodeRegistration {
    uint64_t methodPointersCount = 0;
    uint64_t methodPointers = 0;
    uint64_t delegateWrappersFromNativeToManagedCount = 0;
    uint64_t delegateWrappersFromNativeToManaged = 0;
    uint64_t reversePInvokeWrapperCount = 0;
    uint64_t reversePInvokeWrappers = 0;
    uint64_t delegateWrappersFromManagedToNativeCount = 0;
    uint64_t delegateWrappersFromManagedToNative = 0;
    uint64_t marshalingFunctionsCount = 0;
    uint64_t marshalingFunctions = 0;
    uint64_t ccwMarshalingFunctionsCount = 0;
    uint64_t ccwMarshalingFunctions = 0;
    uint64_t genericMethodPointersCount = 0;
    uint64_t genericMethodPointers = 0;
    uint64_t genericAdjustorThunks = 0;
    uint64_t invokerPointersCount = 0;
    uint64_t invokerPointers = 0;
    uint64_t customAttributeCount = 0;
    uint64_t customAttributeGenerators = 0;
    uint64_t guidCount = 0;
    uint64_t guids = 0;
    uint64_t unresolvedVirtualCallCount = 0;
    uint64_t unresolvedVirtualCallPointers = 0;
    uint64_t unresolvedInstanceCallPointers = 0;
    uint64_t unresolvedStaticCallPointers = 0;
    uint64_t interopDataCount = 0;
    uint64_t interopData = 0;
    uint64_t windowsRuntimeFactoryCount = 0;
    uint64_t windowsRuntimeFactoryTable = 0;
    uint64_t codeGenModulesCount = 0;
    uint64_t codeGenModules = 0;

    static Il2CppCodeRegistration read(BinaryStream& s, double ver) {
        Il2CppCodeRegistration r;
        if (ver <= 24.1) {
            r.methodPointersCount = s.ruptr();
            r.methodPointers      = s.ruptr();
        }
        if (ver <= 21.0) {
            r.delegateWrappersFromNativeToManagedCount = s.ruptr();
            r.delegateWrappersFromNativeToManaged      = s.ruptr();
        }
        if (ver >= 22.0) {
            r.reversePInvokeWrapperCount = s.ruptr();
            r.reversePInvokeWrappers     = s.ruptr();
        }
        if (ver <= 22.0) {
            r.delegateWrappersFromManagedToNativeCount = s.ruptr();
            r.delegateWrappersFromManagedToNative      = s.ruptr();
            r.marshalingFunctionsCount = s.ruptr();
            r.marshalingFunctions      = s.ruptr();
        }
        if (ver >= 21.0 && ver <= 22.0) {
            r.ccwMarshalingFunctionsCount = s.ruptr();
            r.ccwMarshalingFunctions      = s.ruptr();
        }
        r.genericMethodPointersCount = s.ruptr();
        r.genericMethodPointers      = s.ruptr();
        bool hasAdjustor = (ver >= 24.5 && ver <= 24.5) || ver >= 27.1;
        if (hasAdjustor) r.genericAdjustorThunks = s.ruptr();
        r.invokerPointersCount = s.ruptr();
        r.invokerPointers      = s.ruptr();
        if (ver <= 24.5) {
            r.customAttributeCount      = s.ruptr();
            r.customAttributeGenerators = s.ruptr();
        }
        if (ver >= 21.0 && ver <= 22.0) {
            r.guidCount = s.ruptr();
            r.guids     = s.ruptr();
        }
        if (ver >= 22.0) {
            r.unresolvedVirtualCallCount    = s.ruptr();
            r.unresolvedVirtualCallPointers = s.ruptr();
        }
        if (ver >= 29.1) {
            r.unresolvedInstanceCallPointers = s.ruptr();
            r.unresolvedStaticCallPointers   = s.ruptr();
        }
        if (ver >= 23.0) {
            r.interopDataCount = s.ruptr();
            r.interopData      = s.ruptr();
        }
        if (ver >= 24.3) {
            r.windowsRuntimeFactoryCount = s.ruptr();
            r.windowsRuntimeFactoryTable = s.ruptr();
        }
        if (ver >= 24.2) {
            r.codeGenModulesCount = s.ruptr();
            r.codeGenModules      = s.ruptr();
        }
        return r;
    }
};

struct Il2CppMetadataRegistration {
    int64_t  genericClassesCount = 0;
    uint64_t genericClasses = 0;
    int64_t  genericInstsCount = 0;
    uint64_t genericInsts = 0;
    int64_t  genericMethodTableCount = 0;
    uint64_t genericMethodTable = 0;
    int64_t  typesCount = 0;
    uint64_t types = 0;
    int64_t  methodSpecsCount = 0;
    uint64_t methodSpecs = 0;
    int64_t  methodReferencesCount = 0;
    uint64_t methodReferences = 0;
    int64_t  fieldOffsetsCount = 0;
    uint64_t fieldOffsets = 0;
    int64_t  typeDefinitionsSizesCount = 0;
    uint64_t typeDefinitionsSizes = 0;
    uint64_t metadataUsagesCount = 0;
    uint64_t metadataUsages = 0;

    static Il2CppMetadataRegistration read(BinaryStream& s, double ver) {
        Il2CppMetadataRegistration r;
        r.genericClassesCount   = s.riptr();
        r.genericClasses        = s.ruptr();
        r.genericInstsCount     = s.riptr();
        r.genericInsts          = s.ruptr();
        r.genericMethodTableCount = s.riptr();
        r.genericMethodTable    = s.ruptr();
        r.typesCount            = s.riptr();
        r.types                 = s.ruptr();
        r.methodSpecsCount      = s.riptr();
        r.methodSpecs           = s.ruptr();
        if (ver <= 16.0) {
            r.methodReferencesCount = s.riptr();
            r.methodReferences      = s.ruptr();
        }
        r.fieldOffsetsCount         = s.riptr();
        r.fieldOffsets              = s.ruptr();
        r.typeDefinitionsSizesCount = s.riptr();
        r.typeDefinitionsSizes      = s.ruptr();
        if (ver >= 19.0) {
            r.metadataUsagesCount = s.ruptr();
            r.metadataUsages      = s.ruptr();
        }
        return r;
    }
};

struct Il2CppCodeGenModule {
    uint64_t moduleName = 0;
    int64_t  methodPointerCount = 0;
    uint64_t methodPointers = 0;
    int64_t  adjustorThunkCount = 0;
    uint64_t adjustorThunks = 0;
    uint64_t invokerIndices = 0;
    uint64_t reversePInvokeWrapperCount = 0;
    uint64_t reversePInvokeWrapperIndices = 0;
    int64_t  rgctxRangesCount = 0;
    uint64_t rgctxRanges = 0;
    int64_t  rgctxsCount = 0;
    uint64_t rgctxs = 0;
    uint64_t debuggerMetadata = 0;
    uint64_t customAttributeCacheGenerator = 0;
    uint64_t moduleInitializer = 0;
    uint64_t staticConstructorTypeIndices = 0;
    uint64_t metadataRegistration = 0;
    uint64_t codeRegistaration = 0;

    static Il2CppCodeGenModule read(BinaryStream& s, double ver) {
        Il2CppCodeGenModule m;
        m.moduleName        = s.ruptr();
        m.methodPointerCount = s.riptr();
        m.methodPointers    = s.ruptr();
        bool hasAdjustor = (ver >= 24.5 && ver <= 24.5) || ver >= 27.1;
        if (hasAdjustor) {
            m.adjustorThunkCount = s.riptr();
            m.adjustorThunks     = s.ruptr();
        }
        m.invokerIndices              = s.ruptr();
        m.reversePInvokeWrapperCount  = s.ruptr();
        m.reversePInvokeWrapperIndices = s.ruptr();
        m.rgctxRangesCount = s.riptr();
        m.rgctxRanges      = s.ruptr();
        m.rgctxsCount      = s.riptr();
        m.rgctxs           = s.ruptr();
        m.debuggerMetadata = s.ruptr();
        if (ver >= 27.0 && ver <= 27.2) m.customAttributeCacheGenerator = s.ruptr();
        if (ver >= 27.0) {
            m.moduleInitializer            = s.ruptr();
            m.staticConstructorTypeIndices = s.ruptr();
            m.metadataRegistration         = s.ruptr();
            m.codeRegistaration            = s.ruptr();
        }
        return m;
    }
};

// ---- Metadata structs (rewritten to match Project B / CODM layout for v23) ----

struct Il2CppGlobalMetadataHeader {
    uint32_t sanity = 0;
    int32_t  version_num = 0;
    uint32_t stringLiteralOffset = 0;
    int32_t  stringLiteralSize = 0;
    uint32_t stringLiteralDataOffset = 0;
    int32_t  stringLiteralDataSize = 0;
    uint32_t stringOffset = 0;
    int32_t  stringSize = 0;
    uint32_t eventsOffset = 0;
    int32_t  eventsSize = 0;
    uint32_t propertiesOffset = 0;
    int32_t  propertiesSize = 0;
    uint32_t methodsOffset = 0;
    int32_t  methodsSize = 0;
    uint32_t parameterDefaultValuesOffset = 0;
    int32_t  parameterDefaultValuesSize = 0;
    uint32_t fieldDefaultValuesOffset = 0;
    int32_t  fieldDefaultValuesSize = 0;
    uint32_t fieldAndParameterDefaultValueDataOffset = 0;
    int32_t  fieldAndParameterDefaultValueDataSize = 0;
    int32_t  fieldMarshaledSizesOffset = 0;
    int32_t  fieldMarshaledSizesSize = 0;
    uint32_t parametersOffset = 0;
    int32_t  parametersSize = 0;
    uint32_t fieldsOffset = 0;
    int32_t  fieldsSize = 0;
    uint32_t genericParametersOffset = 0;
    int32_t  genericParametersSize = 0;
    uint32_t genericParameterConstraintsOffset = 0;
    int32_t  genericParameterConstraintsSize = 0;
    uint32_t genericContainersOffset = 0;
    int32_t  genericContainersSize = 0;
    uint32_t nestedTypesOffset = 0;
    int32_t  nestedTypesSize = 0;
    uint32_t interfacesOffset = 0;
    int32_t  interfacesSize = 0;
    uint32_t vtableMethodsOffset = 0;
    int32_t  vtableMethodsSize = 0;
    int32_t  interfaceOffsetsOffset = 0;
    int32_t  interfaceOffsetsSize = 0;
    uint32_t typeDefinitionsOffset = 0;
    int32_t  typeDefinitionsSize = 0;
    uint32_t rgctxEntriesOffset = 0;
    int32_t  rgctxEntriesCount = 0;
    uint32_t imagesOffset = 0;
    int32_t  imagesSize = 0;
    uint32_t assembliesOffset = 0;
    int32_t  assembliesSize = 0;
    uint32_t metadataUsageListsOffset = 0;
    int32_t  metadataUsageListsCount = 0;
    uint32_t metadataUsagePairsOffset = 0;
    int32_t  metadataUsagePairsCount = 0;
    uint32_t fieldRefsOffset = 0;
    int32_t  fieldRefsSize = 0;
    int32_t  referencedAssembliesOffset = 0;
    int32_t  referencedAssembliesSize = 0;
    uint32_t attributesInfoOffset = 0;
    int32_t  attributesInfoCount = 0;
    uint32_t attributeTypesOffset = 0;
    int32_t  attributeTypesCount = 0;
    uint32_t attributeDataOffset = 0;
    int32_t  attributeDataSize = 0;
    uint32_t attributeDataRangeOffset = 0;
    int32_t  attributeDataRangeSize = 0;
    int32_t  unresolvedVirtualCallParameterTypesOffset = 0;
    int32_t  unresolvedVirtualCallParameterTypesSize = 0;
    int32_t  unresolvedVirtualCallParameterRangesOffset = 0;
    int32_t  unresolvedVirtualCallParameterRangesSize = 0;
    int32_t  windowsRuntimeTypeNamesOffset = 0;
    int32_t  windowsRuntimeTypeNamesSize = 0;
    int32_t  windowsRuntimeStringsOffset = 0;
    int32_t  windowsRuntimeStringsSize = 0;
    int32_t  exportedTypeDefinitionsOffset = 0;
    int32_t  exportedTypeDefinitionsSize = 0;

    static Il2CppGlobalMetadataHeader read(BinaryStream& s, double ver) {
        Il2CppGlobalMetadataHeader h;
        s.seek(0);
        h.sanity      = s.ru32();
        h.version_num = s.ri32();
        h.stringLiteralOffset     = s.ru32();
        h.stringLiteralSize       = s.ri32();
        h.stringLiteralDataOffset = s.ru32();
        h.stringLiteralDataSize   = s.ri32();
        h.stringOffset = s.ru32();
        h.stringSize   = s.ri32();
        h.eventsOffset = s.ru32();
        h.eventsSize   = s.ri32();
        h.propertiesOffset = s.ru32();
        h.propertiesSize   = s.ri32();
        h.methodsOffset = s.ru32();
        h.methodsSize   = s.ri32();
        h.parameterDefaultValuesOffset = s.ru32();
        h.parameterDefaultValuesSize   = s.ri32();
        h.fieldDefaultValuesOffset = s.ru32();
        h.fieldDefaultValuesSize   = s.ri32();
        h.fieldAndParameterDefaultValueDataOffset = s.ru32();
        h.fieldAndParameterDefaultValueDataSize   = s.ri32();
        h.fieldMarshaledSizesOffset = s.ri32();
        h.fieldMarshaledSizesSize   = s.ri32();
        h.parametersOffset = s.ru32();
        h.parametersSize   = s.ri32();
        h.fieldsOffset = s.ru32();
        h.fieldsSize   = s.ri32();
        h.genericParametersOffset = s.ru32();
        h.genericParametersSize   = s.ri32();
        h.genericParameterConstraintsOffset = s.ru32();
        h.genericParameterConstraintsSize   = s.ri32();
        h.genericContainersOffset = s.ru32();
        h.genericContainersSize   = s.ri32();
        h.nestedTypesOffset = s.ru32();
        h.nestedTypesSize   = s.ri32();
        h.interfacesOffset = s.ru32();
        h.interfacesSize   = s.ri32();
        h.vtableMethodsOffset = s.ru32();
        h.vtableMethodsSize   = s.ri32();
        h.interfaceOffsetsOffset = s.ri32();
        h.interfaceOffsetsSize   = s.ri32();
        h.typeDefinitionsOffset = s.ru32();
        h.typeDefinitionsSize   = s.ri32();
        if (ver <= 24.1) {
            h.rgctxEntriesOffset = s.ru32();
            h.rgctxEntriesCount  = s.ri32();
        }
        h.imagesOffset    = s.ru32();
        h.imagesSize      = s.ri32();
        h.assembliesOffset = s.ru32();
        h.assembliesSize   = s.ri32();
        if (ver >= 19.0 && ver <= 24.5) {
            h.metadataUsageListsOffset = s.ru32();
            h.metadataUsageListsCount  = s.ri32();
            h.metadataUsagePairsOffset = s.ru32();
            h.metadataUsagePairsCount  = s.ri32();
        }
        if (ver >= 19.0) {
            h.fieldRefsOffset = s.ru32();
            h.fieldRefsSize   = s.ri32();
        }
        if (ver >= 20.0) {
            h.referencedAssembliesOffset = s.ri32();
            h.referencedAssembliesSize   = s.ri32();
        }
        if (ver >= 21.0 && ver <= 27.2) {
            h.attributesInfoOffset = s.ru32();
            h.attributesInfoCount  = s.ri32();
            h.attributeTypesOffset = s.ru32();
            h.attributeTypesCount  = s.ri32();
        }
        if (ver >= 29.0) {
            h.attributeDataOffset      = s.ru32();
            h.attributeDataSize        = s.ri32();
            h.attributeDataRangeOffset = s.ru32();
            h.attributeDataRangeSize   = s.ri32();
        }
        if (ver >= 22.0) {
            h.unresolvedVirtualCallParameterTypesOffset = s.ri32();
            h.unresolvedVirtualCallParameterTypesSize   = s.ri32();
            h.unresolvedVirtualCallParameterRangesOffset = s.ri32();
            h.unresolvedVirtualCallParameterRangesSize   = s.ri32();
        }
        if (ver >= 23.0) {
            h.windowsRuntimeTypeNamesOffset = s.ri32();
            h.windowsRuntimeTypeNamesSize   = s.ri32();
        }
        if (ver >= 27.0) {
            h.windowsRuntimeStringsOffset = s.ri32();
            h.windowsRuntimeStringsSize   = s.ri32();
        }
        if (ver >= 24.0) {
            h.exportedTypeDefinitionsOffset = s.ri32();
            h.exportedTypeDefinitionsSize   = s.ri32();
        }
        return h;
    }
};

// ---- CODM metadata structs (Project B layout) ----

struct Il2CppImageDefinition {
    uint32_t nameIndex = 0;
    int32_t  assemblyIndex = 0;
    int32_t  typeStart = 0;
    uint32_t typeCount = 0;
    int32_t  entryPointIndex = 0;

    static Il2CppImageDefinition read(BinaryStream& s, double ver) {
        Il2CppImageDefinition d;
        d.nameIndex     = s.ru32();
        d.assemblyIndex = s.ri32();
        d.typeStart     = s.ri32();
        d.typeCount     = s.ru32();
        d.entryPointIndex = s.ri32();
        return d;
    }
    static size_t structSize(double ver) {
        (void)ver;
        return 20;
    }
};

struct Il2CppAssemblyNameDefinition {
    uint32_t nameIndex = 0;
    uint32_t cultureIndex = 0;
    int32_t  hashValueIndex = 0;
    uint32_t publicKeyIndex = 0;
    uint32_t hash_alg = 0;
    int32_t  hash_len = 0;
    uint32_t flags = 0;
    int32_t  major = 0;
    int32_t  minor = 0;
    int32_t  build = 0;
    int32_t  revision = 0;
    uint8_t  public_key_token[8] = {};

    static Il2CppAssemblyNameDefinition read(BinaryStream& s, double ver) {
        (void)ver;
        Il2CppAssemblyNameDefinition a;
        a.nameIndex    = s.ru32();
        a.cultureIndex = s.ru32();
        a.hashValueIndex   = s.ri32();
        a.publicKeyIndex   = s.ru32();
        a.hash_alg   = s.ru32();
        a.hash_len   = s.ri32();
        a.flags      = s.ru32();
        a.major      = s.ri32();
        a.minor      = s.ri32();
        a.build      = s.ri32();
        a.revision   = s.ri32();
        memcpy(a.public_key_token, s.buf.data() + s.pos, 8);
        s.pos += 8;
        return a;
    }
    static size_t structSize(double ver) {
        (void)ver;
        return 52;
    }
};

struct Il2CppAssemblyDefinition {
    int32_t  imageIndex = 0;
    int16_t  customAttributeIndex = 0;
    int32_t  referencedAssemblyStart = 0;
    int32_t  referencedAssemblyCount = 0;
    Il2CppAssemblyNameDefinition aname;

    static Il2CppAssemblyDefinition read(BinaryStream& s, double ver) {
        Il2CppAssemblyDefinition a;
        a.imageIndex           = s.ri32();
        a.customAttributeIndex = s.ri16();
        a.referencedAssemblyStart = s.ri32();
        a.referencedAssemblyCount = s.ri32();
        a.aname = Il2CppAssemblyNameDefinition::read(s, ver);
        return a;
    }
    static size_t structSize(double ver) {
        return 4 + 2 + 4 + 4 + Il2CppAssemblyNameDefinition::structSize(ver);
    }
};

struct Il2CppTypeDefinition {
    uint32_t nameIndex = 0;
    uint32_t namespaceIndex = 0;
    int32_t  byvalTypeIndex = 0;
    int32_t  byrefTypeIndex = 0;
    int32_t  declaringTypeIndex = 0;
    int32_t  parentIndex = 0;
    int32_t  elementTypeIndex = 0;
    uint32_t flags = 0;
    int32_t  fieldStart = 0;
    int32_t  methodStart = 0;
    int32_t  vtableStart = 0;
    int16_t  customAttributeIndex = 0;
    int16_t  rgctxStartIndex = 0;
    int16_t  rgctxCount = 0;
    int16_t  genericContainerIndex = 0;
    int16_t  eventStart = 0;
    uint16_t propertyStart = 0;
    int16_t  nestedTypesStart = 0;
    int16_t  interfacesStart = 0;
    int16_t  interfaceOffsetsStart = 0;
    uint16_t method_count = 0;
    uint16_t property_count = 0;
    uint16_t field_count = 0;
    uint16_t event_count = 0;
    uint16_t nested_type_count = 0;
    uint16_t vtable_count = 0;
    uint16_t interfaces_count = 0;
    uint16_t interface_offsets_count = 0;
    uint16_t bitfield = 0;

    bool IsValueType() const { return (bitfield & 0x1) == 1; }
    bool IsEnum() const { return ((bitfield >> 1) & 0x1) == 1; }

    static Il2CppTypeDefinition read(BinaryStream& s, double ver) {
        (void)ver;
        Il2CppTypeDefinition d;
        d.nameIndex      = s.ru32();
        d.namespaceIndex = s.ru32();
        d.byvalTypeIndex = s.ri32();
        d.byrefTypeIndex = s.ri32();
        d.declaringTypeIndex = s.ri32();
        d.parentIndex        = s.ri32();
        d.elementTypeIndex   = s.ri32();
        d.flags          = s.ru32();
        d.fieldStart     = s.ri32();
        d.methodStart    = s.ri32();
        d.vtableStart    = s.ri32();
        d.customAttributeIndex = s.ri16();
        d.rgctxStartIndex     = s.ri16();
        d.rgctxCount          = s.ri16();
        d.genericContainerIndex = s.ri16();
        d.eventStart     = s.ri16();
        d.propertyStart  = s.ru16();
        d.nestedTypesStart = s.ri16();
        d.interfacesStart  = s.ri16();
        d.interfaceOffsetsStart = s.ri16();
        d.method_count          = s.ru16();
        d.property_count        = s.ru16();
        d.field_count           = s.ru16();
        d.event_count           = s.ru16();
        d.nested_type_count     = s.ru16();
        d.vtable_count          = s.ru16();
        d.interfaces_count      = s.ru16();
        d.interface_offsets_count = s.ru16();
        d.bitfield = s.ru16();
        return d;
    }
    static size_t structSize(double ver) {
        (void)ver;
        return 80;
    }
};

struct Il2CppMethodDefinition {
    uint32_t nameIndex = 0;
    int32_t  methodIndex = 0;
    int32_t  returnType = 0;
    int32_t  parameterStart = 0;
    uint32_t token = 0;
    uint16_t declaringType = 0;
    int16_t  customAttributeIndex = 0;
    int16_t  genericContainerIndex = 0;
    int16_t  invokerIndex = 0;
    int16_t  delegateWrapperIndex = 0;
    int16_t  rgctxStartIndex = 0;
    int16_t  rgctxCount = 0;
    uint16_t flags = 0;
    uint16_t iflags = 0;
    uint16_t slot = 0;
    uint16_t parameterCount = 0;

    static Il2CppMethodDefinition read(BinaryStream& s, double ver) {
        (void)ver;
        Il2CppMethodDefinition m;
        m.nameIndex      = s.ru32();
        m.methodIndex    = s.ri32();
        m.returnType     = s.ri32();
        m.parameterStart = s.ri32();
        m.token          = s.ru32();
        m.declaringType  = s.ru16();
        m.customAttributeIndex = s.ri16();
        m.genericContainerIndex = s.ri16();
        m.invokerIndex          = s.ri16();
        m.delegateWrapperIndex  = s.ri16();
        m.rgctxStartIndex = s.ri16();
        m.rgctxCount      = s.ri16();
        m.flags           = s.ru16();
        m.iflags          = s.ru16();
        m.slot            = s.ru16();
        m.parameterCount  = s.ru16();
        return m;
    }
    static size_t structSize(double ver) {
        (void)ver;
        return 42;
    }
};

struct Il2CppParameterDefinition {
    uint32_t nameIndex = 0;
    int16_t  customAttributeIndex = 0;
    int32_t  typeIndex = 0;

    static Il2CppParameterDefinition read(BinaryStream& s, double ver) {
        (void)ver;
        Il2CppParameterDefinition p;
        p.nameIndex = s.ru32();
        p.customAttributeIndex = s.ri16();
        p.typeIndex = s.ri32();
        return p;
    }
    static size_t structSize(double ver) {
        (void)ver;
        return 10;
    }
};

struct Il2CppFieldDefinition {
    uint32_t nameIndex = 0;
    int32_t  typeIndex = 0;
    int16_t  customAttributeIndex = 0;

    static Il2CppFieldDefinition read(BinaryStream& s, double ver) {
        (void)ver;
        Il2CppFieldDefinition f;
        f.nameIndex = s.ru32();
        f.typeIndex = s.ri32();
        f.customAttributeIndex = s.ri16();
        return f;
    }
    static size_t structSize(double ver) {
        (void)ver;
        return 10;
    }
};

struct Il2CppFieldDefaultValue {
    int32_t fieldIndex = 0;
    int32_t typeIndex  = 0;
    int32_t dataIndex  = 0;

    static Il2CppFieldDefaultValue read(BinaryStream& s) {
        Il2CppFieldDefaultValue f;
        f.fieldIndex = s.ri32();
        f.typeIndex  = s.ri32();
        f.dataIndex  = s.ri32();
        return f;
    }
    static size_t structSize() { return 12; }
};

struct Il2CppParameterDefaultValue {
    int32_t parameterIndex = 0;
    int32_t typeIndex  = 0;
    int32_t dataIndex  = 0;

    static Il2CppParameterDefaultValue read(BinaryStream& s) {
        Il2CppParameterDefaultValue p;
        p.parameterIndex = s.ri32();
        p.typeIndex      = s.ri32();
        p.dataIndex      = s.ri32();
        return p;
    }
    static size_t structSize() { return 12; }
};

struct Il2CppPropertyDefinition {
    uint32_t nameIndex = 0;
    int16_t  get = 0;
    int16_t  set = 0;
    uint32_t attrs = 0;

    static Il2CppPropertyDefinition read(BinaryStream& s, double ver) {
        (void)ver;
        Il2CppPropertyDefinition p;
        p.nameIndex = s.ru32();
        p.get       = s.ri16();
        p.set       = s.ri16();
        p.attrs     = s.ru32();
        return p;
    }
    static size_t structSize(double ver) {
        (void)ver;
        return 12;
    }
};

struct Il2CppEventDefinition {
    uint32_t nameIndex = 0;
    int32_t  typeIndex = 0;
    int16_t  add = 0;
    int16_t  remove = 0;
    int16_t  raise = 0;
    int16_t  customAttributeIndex = 0;

    static Il2CppEventDefinition read(BinaryStream& s, double ver) {
        (void)ver;
        Il2CppEventDefinition e;
        e.nameIndex = s.ru32();
        e.typeIndex = s.ri32();
        e.add       = s.ri16();
        e.remove    = s.ri16();
        e.raise     = s.ri16();
        e.customAttributeIndex = s.ri16();
        return e;
    }
    static size_t structSize(double ver) {
        (void)ver;
        return 16;
    }
};

struct Il2CppGenericContainer {
    int32_t  ownerIndex = 0;
    int32_t  genericParameterStart = 0;
    int16_t  type_argc = 0;
    int16_t  is_method = 0;

    static Il2CppGenericContainer read(BinaryStream& s) {
        Il2CppGenericContainer g;
        g.ownerIndex            = s.ri32();
        g.genericParameterStart = s.ri32();
        g.type_argc             = s.ri16();
        g.is_method             = s.ri16();
        return g;
    }
    static size_t structSize() { return 12; }
};

struct Il2CppGenericParameter {
    uint32_t nameIndex = 0;
    int16_t  ownerIndex = 0;
    int16_t  constraintsStart = 0;
    int16_t  constraintsCount = 0;
    uint16_t num = 0;
    uint16_t flags = 0;

    static Il2CppGenericParameter read(BinaryStream& s) {
        Il2CppGenericParameter g;
        g.nameIndex        = s.ru32();
        g.ownerIndex       = s.ri16();
        g.constraintsStart = s.ri16();
        g.constraintsCount = s.ri16();
        g.num              = s.ru16();
        g.flags            = s.ru16();
        return g;
    }
    static size_t structSize() { return 14; }
};

struct Il2CppFieldRef {
    int32_t typeIndex  = 0;
    int16_t fieldIndex = 0;

    static Il2CppFieldRef read(BinaryStream& s) {
        Il2CppFieldRef f;
        f.typeIndex  = s.ri32();
        f.fieldIndex = s.ri16();
        return f;
    }
    static size_t structSize() { return 6; }
};

struct Il2CppStringLiteral {
    uint32_t length = 0;
    int32_t  dataIndex = 0;

    static Il2CppStringLiteral read(BinaryStream& s) {
        Il2CppStringLiteral sl;
        sl.length    = s.ru32();
        sl.dataIndex = s.ri32();
        return sl;
    }
    static size_t structSize() { return 8; }
};

struct Il2CppCustomAttributeTypeRange {
    int16_t start = 0;
    int16_t count = 0;

    static Il2CppCustomAttributeTypeRange read(BinaryStream& s, double ver) {
        (void)ver;
        Il2CppCustomAttributeTypeRange r;
        r.start = s.ri16();
        r.count = s.ri16();
        return r;
    }
    static size_t structSize(double ver) {
        (void)ver;
        return 4;
    }
};

struct Il2CppCustomAttributeDataRange {
    uint32_t token = 0;
    uint32_t startOffset = 0;

    static Il2CppCustomAttributeDataRange read(BinaryStream& s) {
        Il2CppCustomAttributeDataRange r;
        r.token       = s.ru32();
        r.startOffset = s.ru32();
        return r;
    }
    static size_t structSize() { return 8; }
};

struct Il2CppMetadataUsageList {
    uint32_t start = 0;
    uint16_t count = 0;

    static Il2CppMetadataUsageList read(BinaryStream& s) {
        Il2CppMetadataUsageList m;
        m.start = s.ru32();
        m.count = s.ru16();
        return m;
    }
    static size_t structSize() { return 6; }
};

struct Il2CppMetadataUsagePair {
    uint32_t destinationIndex = 0;
    uint32_t encodedSourceIndex = 0;

    static Il2CppMetadataUsagePair read(BinaryStream& s) {
        Il2CppMetadataUsagePair m;
        m.destinationIndex  = s.ru32();
        m.encodedSourceIndex = s.ru32();
        return m;
    }
    static size_t structSize() { return 8; }
};

struct Il2CppRGCTXDefinition {
    Il2CppRGCTXDataType type_val = Il2CppRGCTXDataType::IL2CPP_RGCTX_DATA_INVALID;
    int32_t  data_pre29 = 0;
    uint64_t data_post29 = 0;

    static Il2CppRGCTXDefinition read(BinaryStream& s, double ver) {
        Il2CppRGCTXDefinition r;
        if (ver >= 27.2) {
            uint64_t type_post = s.ru64();
            r.type_val = (Il2CppRGCTXDataType)(type_post == 0 ? 0 : (int32_t)type_post);
            r.data_post29 = s.ru64();
        } else {
            r.type_val  = (Il2CppRGCTXDataType)s.ri32();
            r.data_pre29 = s.ri32();
        }
        return r;
    }
    static size_t structSize(double ver) {
        return (ver >= 27.2) ? 16 : 8;
    }
};
