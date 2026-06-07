#include "Dumper.h"
#include "Il2CppConstants.h"
#include "CustomAttributeReader.h"
#include "obfuscate.h"
#include <cstdio>

static std::string escapeStr(const std::string& s) {
    std::string r;
    for (char c : s) {
        if      (c=='\n') r+="\\n";
        else if (c=='\r') r+="\\r";
        else if (c=='\t') r+="\\t";
        else if (c=='"')  r+="\\\"";
        else if (c=='\\') r+="\\\\";
        else r+=c;
    }
    return r;
}

static std::string blobToString(const BlobValue& bv) {
    if (bv.isNull) return "null";
    return std::visit([&](auto&& v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T,std::monostate>) return "";
        else if constexpr (std::is_same_v<T,bool>)      return v?"true":"false";
        else if constexpr (std::is_same_v<T,std::string>) return "\""+escapeStr(v)+"\"";
        else if constexpr (std::is_same_v<T,Il2CppType*>) return "";
        else if constexpr (std::is_same_v<T,std::nullptr_t>) return "null";
        else if constexpr (std::is_same_v<T,std::vector<BlobValue>>) return "{...}";
        else if constexpr (std::is_same_v<T,float>) { char b[32]; snprintf(b,sizeof(b),"%g",(double)v); return b; }
        else if constexpr (std::is_same_v<T,double>) { char b[32]; snprintf(b,sizeof(b),"%g",v); return b; }
        else if constexpr (std::is_arithmetic_v<T>) return std::to_string(v);
        return "";
    }, bv.value);
}

std::string Dumper::getCustomAttribute(const Il2CppImageDefinition& imageDef,
                                       int32_t customAttributeIndex, uint32_t token,
                                       const std::string& padding) {
    if (il2Cpp->version < 21.0) return "";
    int32_t attrIdx = -1;
    try { attrIdx = metadata->getCustomAttributeIndex(imageDef, customAttributeIndex, token); }
    catch (...) { return ""; }
    if (attrIdx < 0) return "";

    std::string result;

    if (il2Cpp->version < 29.0) {
        try {
            if ((size_t)attrIdx >= executor->customAttributeGenerators.size()) return "";
            uint64_t mp = executor->customAttributeGenerators[(size_t)attrIdx];
            uint64_t fixedPtr = il2Cpp->getRVA(mp);
            if ((size_t)attrIdx >= metadata->attributeTypeRanges.size()) return "";
            auto& atr = metadata->attributeTypeRanges[(size_t)attrIdx];
            for (int32_t i = 0; i < atr.count; i++) {
                int32_t idx = atr.start + i;
                if (idx < 0 || (size_t)idx >= metadata->attributeTypes.size()) continue;
                int32_t ti = metadata->attributeTypes[(size_t)idx];
                if (ti < 0 || (size_t)ti >= il2Cpp->types.size()) continue;
                std::string tn = executor->getTypeName(il2Cpp->types[(size_t)ti], false, false);
                char buf[256];
                snprintf(buf, sizeof(buf), "%s[%s] // RVA: 0x%llX Offset: 0x%llX VA: 0x%llX\n",
                    padding.c_str(), tn.c_str(),
                    (unsigned long long)fixedPtr,
                    (unsigned long long)(mp ? il2Cpp->mapVATR(mp) : 0),
                    (unsigned long long)mp);
                result += buf;
            }
        } catch (...) {}
    } else {
        try {
            if ((size_t)attrIdx + 1 >= metadata->attributeDataRanges.size()) return "";
            auto& sr = metadata->attributeDataRanges[(size_t)attrIdx];
            auto& er = metadata->attributeDataRanges[(size_t)attrIdx + 1];
            if (er.startOffset <= sr.startOffset) return "";
            uint32_t sz = er.startOffset - sr.startOffset;
            metadata->seek(metadata->header.attributeDataOffset + sr.startOffset);
            auto buff = metadata->readBytes((size_t)sz);
            if (buff.empty()) return "";

            CustomAttributeReader reader(executor, buff);
            if (reader.count == 0) return "";

            for (uint32_t i = 0; i < reader.count; i++) {
                std::string s = reader.getString();
                if (!s.empty()) result += padding + s + "\n";
            }
        } catch (...) {}
    }
    return result;
}

std::string Dumper::getModifiers(const Il2CppMethodDefinition& methodDef) {
    size_t key = (size_t)(&methodDef - metadata->methodDefs.data());
    auto it = methodModifiersCache.find(key);
    if (it != methodModifiersCache.end()) return it->second;
    std::string s;
    int acc = methodDef.flags & METHOD_ATTRIBUTE_MEMBER_ACCESS_MASK;
    switch (acc) {
        case METHOD_ATTRIBUTE_PRIVATE:       s+="private ";           break;
        case METHOD_ATTRIBUTE_PUBLIC:        s+="public ";            break;
        case METHOD_ATTRIBUTE_FAMILY:        s+="protected ";         break;
        case METHOD_ATTRIBUTE_ASSEM:
        case METHOD_ATTRIBUTE_FAM_AND_ASSEM: s+="internal ";          break;
        case METHOD_ATTRIBUTE_FAM_OR_ASSEM:  s+="protected internal "; break;
    }
    if (methodDef.flags & METHOD_ATTRIBUTE_STATIC)   s+="static ";
    if (methodDef.flags & METHOD_ATTRIBUTE_ABSTRACT) {
        s+="abstract ";
        if ((methodDef.flags & METHOD_ATTRIBUTE_VTABLE_LAYOUT_MASK)==METHOD_ATTRIBUTE_REUSE_SLOT) s+="override ";
    } else if (methodDef.flags & METHOD_ATTRIBUTE_FINAL) {
        if ((methodDef.flags & METHOD_ATTRIBUTE_VTABLE_LAYOUT_MASK)==METHOD_ATTRIBUTE_REUSE_SLOT) s+="sealed override ";
    } else if (methodDef.flags & METHOD_ATTRIBUTE_VIRTUAL) {
        s += ((methodDef.flags & METHOD_ATTRIBUTE_VTABLE_LAYOUT_MASK)==METHOD_ATTRIBUTE_NEW_SLOT) ? "virtual " : "override ";
    }
    if (methodDef.flags & METHOD_ATTRIBUTE_PINVOKE_IMPL) s+="extern ";
    return methodModifiersCache[key] = s;
}

void Dumper::dump(const DumpConfig& cfg, const std::string& outputDir) {
    std::string outPath = outputDir + "dump.cs";
    FILE* f = fopen(outPath.c_str(), "wb");
    if (!f) { fprintf(stderr, "Cannot open output: %s\n", outPath.c_str()); return; }

    // Write credit header at top of file
    fputs(OBFUSCATE(R"(

// ███╗   ███╗ █████╗  ██████╗ ██╗ ██████╗
// ████╗ ████║██╔══██╗██╔════╝ ██║██╔════╝
// ██╔████╔██║███████║██║  ███╗██║██║
// ██║╚██╔╝██║██╔══██║██║   ██║██║██║
// ██║ ╚═╝ ██║██║  ██║╚██████╔╝██║╚██████╗
// ╚═╝     ╚═╝╚═╝  ╚═╝ ╚═════╝ ╚═╝ ╚═════╝
//
// ███╗   ███╗ ██████╗ ██████╗ ███████╗
// ████╗ ████║██╔═══██╗██╔══██╗██╔════╝
// ██╔████╔██║██║   ██║██║  ██║███████╗
// ██║╚██╔╝██║██║   ██║██║  ██║╚════██║
// ██║ ╚═╝ ██║╚██████╔╝██████╔╝███████║
// ╚═╝     ╚═╝ ╚═════╝ ╚═════╝ ╚══════╝
//
// ============================================================
//                 CODM DUMPER - MAGIC MODS
// ============================================================
//
//  Telegram Channel : https://t.me/retiredgamermods
//  Telegram Group   : https://t.me/magicmodschat
//  YouTube          : https://youtube.com/@magicmods-u5k
//
// ============================================================

)"), f);

    for (size_t i = 0; i < metadata->imageDefs.size(); i++) {
        auto& img = metadata->imageDefs[i];
        fprintf(f, "// Image %zu: %s - %d\n", i,
            metadata->getStringFromIndex(img.nameIndex).c_str(), img.typeStart);
    }

    for (auto& imageDef : metadata->imageDefs) {
        std::string imageName;
        try { imageName = metadata->getStringFromIndex(imageDef.nameIndex); } catch (...) { continue; }

        int typeEnd = imageDef.typeStart + (int)imageDef.typeCount;
        for (int tdi = imageDef.typeStart; tdi < typeEnd; tdi++) {
            try {
                auto& td = metadata->typeDefs[(size_t)tdi];

                // Credit before each class
               // fputs(OBFUSCATE("// IL2CPP Dumped -- By Magic Mods\n"), f);

                std::vector<std::string> extends;
                if (td.parentIndex >= 0 && (size_t)td.parentIndex < il2Cpp->types.size()) {
                    try {
                        std::string pn = executor->getTypeName(il2Cpp->types[(size_t)td.parentIndex], false, false);
                        if (!td.IsValueType() && !td.IsEnum() && pn != "object")
                            extends.push_back(pn);
                    } catch (...) {}
                }
                for (int i = 0; i < td.interfaces_count; i++) {
                    try {
                        int ii = metadata->interfaceIndices[(size_t)(td.interfacesStart+i)];
                        if (ii >= 0 && (size_t)ii < il2Cpp->types.size())
                            extends.push_back(executor->getTypeName(il2Cpp->types[(size_t)ii], false, false));
                    } catch (...) {}
                }

                std::string ns;
                try { ns = metadata->getStringFromIndex(td.namespaceIndex); } catch (...) {}
                fprintf(f, "\n// Namespace: %s\n", ns.c_str());

                if (cfg.DumpAttribute) {
                    try {
                        std::string a = getCustomAttribute(imageDef, td.customAttributeIndex, 0);
                        if (!a.empty()) fputs(a.c_str(), f);
                    } catch (...) {}
                    if (td.flags & TYPE_ATTRIBUTE_SERIALIZABLE) fputs("[Serializable]\n", f);
                }

                int vis = td.flags & TYPE_ATTRIBUTE_VISIBILITY_MASK;
                switch (vis) {
                    case TYPE_ATTRIBUTE_PUBLIC:
                    case TYPE_ATTRIBUTE_NESTED_PUBLIC:        fputs("public ", f);             break;
                    case TYPE_ATTRIBUTE_NOT_PUBLIC:
                    case TYPE_ATTRIBUTE_NESTED_FAM_AND_ASSEM:
                    case TYPE_ATTRIBUTE_NESTED_ASSEMBLY:      fputs("internal ", f);           break;
                    case TYPE_ATTRIBUTE_NESTED_PRIVATE:       fputs("private ", f);            break;
                    case TYPE_ATTRIBUTE_NESTED_FAMILY:        fputs("protected ", f);          break;
                    case TYPE_ATTRIBUTE_NESTED_FAM_OR_ASSEM:  fputs("protected internal ", f); break;
                }
                if ((td.flags & TYPE_ATTRIBUTE_ABSTRACT) && (td.flags & TYPE_ATTRIBUTE_SEALED))    fputs("static ", f);
                else if (!(td.flags & TYPE_ATTRIBUTE_INTERFACE) && (td.flags & TYPE_ATTRIBUTE_ABSTRACT)) fputs("abstract ", f);
                else if (!td.IsValueType() && !td.IsEnum() && (td.flags & TYPE_ATTRIBUTE_SEALED)) fputs("sealed ", f);

                if      (td.flags & TYPE_ATTRIBUTE_INTERFACE) fputs("interface ", f);
                else if (td.IsEnum())                          fputs("enum ", f);
                else if (td.IsValueType())                     fputs("struct ", f);
                else                                           fputs("class ", f);

                std::string tn;
                try { tn = executor->getTypeDefName(td, false, true); } catch (...) { tn = "Unknown"; }
                fputs(tn.c_str(), f);
                if (!extends.empty()) {
                    fputs(" : ", f);
                    for (size_t i = 0; i < extends.size(); i++) { if (i) fputs(", ", f); fputs(extends[i].c_str(), f); }
                }
                if (cfg.DumpTypeDefIndex) fprintf(f, " // TypeDefIndex: %d\n{", tdi);
                else fputs("\n{", f);

                if (cfg.DumpField && td.field_count > 0) {
                    fputs("\n\t// Fields\n", f);
                    for (int fi = td.fieldStart; fi < td.fieldStart + td.field_count; fi++) {
                        try {
                            auto& fd = metadata->fieldDefs[(size_t)fi];
                            if ((size_t)fd.typeIndex >= il2Cpp->types.size()) continue;
                            auto& ft = il2Cpp->types[(size_t)fd.typeIndex];
                            bool isStatic=false, isConst=false;
                            if (cfg.DumpAttribute) {
                                try {
                                    std::string a = getCustomAttribute(imageDef, fd.customAttributeIndex, 0, "\t");
                                    if (!a.empty()) fputs(a.c_str(), f);
                                } catch (...) {}
                            }
                            fputs("\t", f);
                            switch (ft.attrs & FIELD_ATTRIBUTE_FIELD_ACCESS_MASK) {
                                case FIELD_ATTRIBUTE_PRIVATE:        fputs("private ", f);            break;
                                case FIELD_ATTRIBUTE_PUBLIC:         fputs("public ", f);             break;
                                case FIELD_ATTRIBUTE_FAMILY:         fputs("protected ", f);          break;
                                case FIELD_ATTRIBUTE_ASSEMBLY:
                                case FIELD_ATTRIBUTE_FAM_AND_ASSEM:  fputs("internal ", f);           break;
                                case FIELD_ATTRIBUTE_FAM_OR_ASSEM:   fputs("protected internal ", f); break;
                            }
                            if (ft.attrs & FIELD_ATTRIBUTE_LITERAL) { isConst=true; fputs("const ", f); }
                            else {
                                if (ft.attrs & FIELD_ATTRIBUTE_STATIC) { isStatic=true; fputs("static ", f); }
                                if (ft.attrs & FIELD_ATTRIBUTE_INIT_ONLY) fputs("readonly ", f);
                            }
                            std::string ftn = executor->getTypeName(ft, false, false);
                            std::string fn  = metadata->getStringFromIndex(fd.nameIndex);
                            fprintf(f, "%s %s", ftn.c_str(), fn.c_str());
                            Il2CppFieldDefaultValue fdv;
                            if (metadata->getFieldDefaultValue(fi, fdv) && fdv.dataIndex != -1) {
                                try {
                                    BlobValue bv;
                                    if (executor->tryGetDefaultValue(fdv.typeIndex, fdv.dataIndex, bv)) {
                                        std::string vs = blobToString(bv);
                                        if (!vs.empty()) fprintf(f, " = %s", vs.c_str());
                                    }
                                } catch (...) {}
                            }
                            if (cfg.DumpFieldOffset && !isConst)
                                fprintf(f, "; // 0x%X\n", il2Cpp->getFieldOffsetFromIndex(tdi, fi-td.fieldStart, fi, td.IsValueType(), isStatic));
                            else fputs(";\n", f);
                        } catch (...) { fputs(";\n", f); }
                    }
                }

                if (cfg.DumpProperty && td.property_count > 0) {
                    fputs("\n\t// Properties\n", f);
                    for (int pi = td.propertyStart; pi < td.propertyStart + td.property_count; pi++) {
                        try {
                            auto& pd = metadata->propertyDefs[(size_t)pi];
                            if (pd.get < 0 && pd.set < 0) continue;

                            if (cfg.DumpAttribute) {
                                try {
                                    std::string a = getCustomAttribute(imageDef, -1, 0, "\t");
                                    if (!a.empty()) fputs(a.c_str(), f);
                                } catch (...) {}
                            }

                            int getIdx = pd.get >= 0 ? pd.get : pd.set;
                            auto& md = metadata->methodDefs[(size_t)(td.methodStart + getIdx)];

                            Il2CppType* retType = nullptr;
                            if (pd.get >= 0 && (size_t)md.returnType < il2Cpp->types.size()) {
                                retType = &il2Cpp->types[(size_t)md.returnType];
                            } else if (pd.set >= 0) {
                                auto& setMd = metadata->methodDefs[(size_t)(td.methodStart + pd.set)];
                                if (setMd.parameterCount > 0) {
                                    auto& pdef = metadata->parameterDefs[(size_t)setMd.parameterStart];
                                    if ((size_t)pdef.typeIndex < il2Cpp->types.size())
                                        retType = &il2Cpp->types[(size_t)pdef.typeIndex];
                                }
                            }
                            if (!retType) continue;

                            std::string ptn = executor->getTypeName(*retType, false, false);
                            std::string pn  = metadata->getStringFromIndex(pd.nameIndex);
                            std::string mods = getModifiers(md);

                            fprintf(f, "\t%s%s %s {", mods.c_str(), ptn.c_str(), pn.c_str());
                            if (pd.get >= 0) fputs(" get;", f);
                            if (pd.set >= 0) fputs(" set;", f);
                            fputs(" }\n", f);
                        } catch (...) {}
                    }
                }

                if (cfg.DumpMethod && td.method_count > 0) {
                    fputs("\n\t// Methods\n", f);
                    for (int mi = td.methodStart; mi < td.methodStart + td.method_count; mi++) {
                        try {
                            fputs("\n", f);
                            auto& md = metadata->methodDefs[(size_t)mi];
                            bool isAbstract = (md.flags & METHOD_ATTRIBUTE_ABSTRACT) != 0;
                            if (cfg.DumpAttribute) {
                                try {
                                    std::string a = getCustomAttribute(imageDef, md.customAttributeIndex, md.token, "\t");
                                    if (!a.empty()) fputs(a.c_str(), f);
                                } catch (...) {}
                            }
                            if (cfg.DumpMethodOffset) {
                                uint64_t mp = 0;
                                try { mp = il2Cpp->getMethodPointer(imageName, md); } catch (...) {}
                                if (!isAbstract && mp > 0) {
                                    uint64_t offset = 0;
                                    try { offset = il2Cpp->mapVATR(mp); } catch (...) {}
                                    fprintf(f, "\t// RVA: 0x%llX Offset: 0x%llX VA: 0x%llX",
                                        (unsigned long long)il2Cpp->getRVA(mp),
                                        (unsigned long long)offset,
                                        (unsigned long long)mp);
                                } else fputs("\t// RVA: -1 Offset: -1", f);
                                if (md.slot != 0xFFFF) fprintf(f, " Slot: %u", (unsigned)md.slot);
                                fputs("\n", f);
                            }
                            fputs("\t", f);
                            fputs(getModifiers(md).c_str(), f);
                            if ((size_t)md.returnType >= il2Cpp->types.size()) { fputs("void Unknown() { }\n", f); continue; }
                            auto& retType = il2Cpp->types[(size_t)md.returnType];
                            std::string mname;
                            try { mname = metadata->getStringFromIndex(md.nameIndex); } catch (...) { mname = "Unknown"; }
                            if (md.genericContainerIndex >= 0 && (size_t)md.genericContainerIndex < metadata->genericContainers.size()) {
                                try { mname += executor->getGenericContainerParams(metadata->genericContainers[(size_t)md.genericContainerIndex]); } catch (...) {}
                            }
                            if (retType.byref) fputs("ref ", f);
                            std::string rtn;
                            try { rtn = executor->getTypeName(retType, false, false); } catch (...) { rtn = "object"; }
                            fprintf(f, "%s %s(", rtn.c_str(), mname.c_str());

                            std::vector<std::string> params;
                            for (int j = 0; j < md.parameterCount; j++) {
                                try {
                                    std::string ps;
                                    auto& pdef = metadata->parameterDefs[(size_t)(md.parameterStart+j)];
                                    if ((size_t)pdef.typeIndex >= il2Cpp->types.size()) continue;
                                    auto& pt = il2Cpp->types[(size_t)pdef.typeIndex];
                                    std::string pn  = metadata->getStringFromIndex(pdef.nameIndex);
                                    std::string ptn = executor->getTypeName(pt, false, false);
                                    if (pt.byref) {
                                        if ((pt.attrs & PARAM_ATTRIBUTE_OUT) && !(pt.attrs & PARAM_ATTRIBUTE_IN)) ps+="out ";
                                        else if (!(pt.attrs & PARAM_ATTRIBUTE_OUT) && (pt.attrs & PARAM_ATTRIBUTE_IN)) ps+="in ";
                                        else ps+="ref ";
                                    } else {
                                        if (pt.attrs & PARAM_ATTRIBUTE_IN)  ps+="[In] ";
                                        if (pt.attrs & PARAM_ATTRIBUTE_OUT) ps+="[Out] ";
                                    }
                                    ps += ptn + " " + pn;
                                    Il2CppParameterDefaultValue pdv;
                                    if (metadata->getParameterDefaultValue((int)(md.parameterStart+j), pdv) && pdv.dataIndex != -1) {
                                        try {
                                            BlobValue bv;
                                            if (executor->tryGetDefaultValue(pdv.typeIndex, pdv.dataIndex, bv)) {
                                                std::string vs = blobToString(bv);
                                                if (!vs.empty()) ps += " = " + vs;
                                            }
                                        } catch (...) {}
                                    }
                                    params.push_back(ps);
                                } catch (...) {}
                            }
                            for (size_t j = 0; j < params.size(); j++) { if (j) fputs(", ", f); fputs(params[j].c_str(), f); }
                            fputs(isAbstract ? ");\n" : ") { }\n", f);

                            auto git = il2Cpp->methodDefinitionMethodSpecs.find(mi);
                            if (git != il2Cpp->methodDefinitionMethodSpecs.end()) {
                                try {
                                    fputs("\t/* GenericInstMethod :\n", f);
                                    std::unordered_map<uint64_t, std::vector<const Il2CppMethodSpec*>> groups;
                                    for (auto& spec : git->second) {
                                        size_t si = (size_t)(&spec - il2Cpp->methodSpecs.data());
                                        uint64_t gmp = 0;
                                        auto it2 = il2Cpp->methodSpecGenericMethodPointers.find(si);
                                        if (it2 != il2Cpp->methodSpecGenericMethodPointers.end()) gmp = it2->second;
                                        groups[gmp].push_back(&spec);
                                    }
                                    for (auto& kv : groups) {
                                        fputs("\t|\n", f);
                                        if (kv.first > 0) {
                                            uint64_t off = 0; try { off = il2Cpp->mapVATR(kv.first); } catch (...) {}
                                            fprintf(f, "\t|-RVA: 0x%llX Offset: 0x%llX VA: 0x%llX\n",
                                                (unsigned long long)il2Cpp->getRVA(kv.first), (unsigned long long)off, (unsigned long long)kv.first);
                                        } else fputs("\t|-RVA: -1 Offset: -1\n", f);
                                        for (auto* sp : kv.second) {
                                            try { auto [stn,smn] = executor->getMethodSpecName(*sp); fprintf(f, "\t|-%s.%s\n", stn.c_str(), smn.c_str()); }
                                            catch (...) {}
                                        }
                                    }
                                    fputs("\t*/\n", f);
                                } catch (...) {}
                            }
                        } catch (...) { fputs(" { }\n", f); }
                    }
                }
                fputs("}\n", f);
            } catch (const std::exception& e) { fprintf(f, "/* %s */\n}\n", e.what()); }
            catch (...) { fputs("/* error */\n}\n", f); }
        }
    }
    fclose(f);
    printf("Done! Output: %s\n", outPath.c_str());
}
