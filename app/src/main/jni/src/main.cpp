#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

#include "Metadata.h"
#include "ElfReader.h"
#include "Il2CppExecutor.h"
#include "Dumper.h"
#include "obfuscate.h"

static void printUsage(const char* prog) {
    fprintf(stderr,
        "Usage: %s <libunity.so> <global-metadata.dat> [output_dir]\n"
        "       %s <libunity.so> <global-metadata.dat> [output_dir] --code-reg <hex> --meta-reg <hex>\n"
        "\n"
        "Options:\n"
        "  --code-reg <hex>   Manual CodeRegistration VA\n"
        "  --meta-reg <hex>   Manual MetadataRegistration VA\n",
        prog, prog);
}

static void clearConsole() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

static void printCreditBanner() {
    clearConsole();

    fputs(OBFUSCATE(R"(

███╗   ███╗ █████╗  ██████╗ ██╗ ██████╗
████╗ ████║██╔══██╗██╔════╝ ██║██╔════╝
██╔████╔██║███████║██║  ███╗██║██║     
██║╚██╔╝██║██╔══██║██║   ██║██║██║     
██║ ╚═╝ ██║██║  ██║╚██████╔╝██║╚██████╗
╚═╝     ╚═╝╚═╝  ╚═╝ ╚═════╝ ╚═╝ ╚═════╝

███╗   ███╗ ██████╗ ██████╗ ███████╗
████╗ ████║██╔═══██╗██╔══██╗██╔════╝
██╔████╔██║██║   ██║██║  ██║███████╗
██║╚██╔╝██║██║   ██║██║  ██║╚════██║
██║ ╚═╝ ██║╚██████╔╝██████╔╝███████║
╚═╝     ╚═╝ ╚═════╝ ╚═════╝ ╚══════╝

============================================================
              CODM DUMPER - MAGIC MODS
============================================================

  [+] Telegram Channel : https://t.me/retiredgamermods
  [+] Telegram Group   : https://t.me/magicmodschat
  [+] YouTube          : https://youtube.com/@magicmods-u5k?si=rQohKMMV225WO0yW

============================================================

)") , stdout);
}

static uint64_t promptHex(const char* name) {
    char buf[64];
    while (true) {
        printf("Enter %s (hex, e.g. 3b4fdc0): ", name);
        fflush(stdout);
        if (!fgets(buf, sizeof(buf), stdin)) return 0;
        char* end;
        uint64_t val = strtoull(buf, &end, 16);
        if (end != buf && val != 0) return val;
        printf("Invalid. Try again.\n");
    }
}

static bool tryInit(Il2CppBase* il, Metadata* meta,
                    uint64_t forceCode, uint64_t forceMeta) {
    il->setProperties(meta->version, meta->metadataUsagesCount);

    if (forceCode != 0 && forceMeta != 0) {
        printf("Using manual addresses:\n");
        printf("CodeRegistration : %llx\n", (unsigned long long)forceCode);
        printf("MetadataRegistration : %llx\n", (unsigned long long)forceMeta);
        il->init(forceCode, forceMeta);
        return true;
    }

    if (il->checkDump()) {
        printf("Detected as dumped ELF.\n");
        printf("Enter the load address (image base) of the dumped library\n");
        printf("(e.g., 77ddaa4000 for a library loaded at 0x77ddaa4000): ");
        fflush(stdout);
        char ibBuf[64];
        if (fgets(ibBuf, sizeof(ibBuf), stdin)) {
            il->imageBase = strtoull(ibBuf, nullptr, 16);
        }
        printf("Image base: 0x%llx\n", (unsigned long long)il->imageBase);
        il->isDumped = true;
        auto* elf = dynamic_cast<ElfReaderBase*>(il);
        if (elf) elf->reload();
    }

    // : count only methods with methodIndex >= 0 (abstract/extern methods are -1)
    int mc = 0;
    for (auto& md : meta->methodDefs)
        if (md.methodIndex >= 0) mc++;
    int tc = (int)meta->typeDefs.size();
    int ic = (int)meta->imageDefs.size();

    auto computeImageBase = [&]() {
        // v24.2+ stores pointer in Il2CppType.data; v23 stores TypeDefIndex
        if (!il->is32bit && il->version >= 24.2) {
            try {
                if (!meta->typeDefs.empty() && !il->types.empty()) {
                    auto& td = meta->typeDefs[0];
                    if ((size_t)td.byvalTypeIndex < il->types.size()) {
                        auto& il2CppType = il->types[(size_t)td.byvalTypeIndex];
                        meta->ImageBase = il2CppType.data - meta->header.typeDefinitionsOffset;
                        printf("Computed ImageBase: 0x%llx\n", (unsigned long long)meta->ImageBase);
                    }
                }
            } catch (...) {}
        }
    };

    //  search order: plusSearch -> search -> symbolSearch -> manual
    if (il->plusSearch(mc, tc, ic)) {
        computeImageBase();
        return true;
    }
    if (il->search()) {
        computeImageBase();
        return true;
    }
    if (il->symbolSearch()) {
        computeImageBase();
        return true;
    }

    fprintf(stderr, "ERROR: Auto scan failed.\n\n");

    printf("Manual input required.\n");
    forceCode = promptHex("CodeRegistration");
    forceMeta = promptHex("MetadataRegistration");

    printf("CodeRegistration : %llx\n", (unsigned long long)forceCode);
    printf("MetadataRegistration : %llx\n", (unsigned long long)forceMeta);
    il->init(forceCode, forceMeta);
    return true;
}

int main(int argc, char** argv) {
    if (argc < 3) { printUsage(argv[0]); return 1; }

    const char* elfPath      = argv[1];
    const char* metadataPath = argv[2];
    const char* outputDir    = "./";
    uint64_t forceCode       = 0;
    uint64_t forceMeta       = 0;

    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--code-reg") == 0 && i+1 < argc)
            forceCode = (uint64_t)strtoull(argv[++i], nullptr, 16);
        else if (strcmp(argv[i], "--meta-reg") == 0 && i+1 < argc)
            forceMeta = (uint64_t)strtoull(argv[++i], nullptr, 16);
        else if (argv[i][0] != '-')
            outputDir = argv[i];
    }

    DumpConfig cfg;
    printCreditBanner();

    std::string outDir = outputDir;
    if (!outDir.empty() && outDir.back() != '/') outDir += '/';

    Metadata* meta = nullptr;
    Il2CppBase* il = nullptr;

    fprintf(stderr, "DEBUG: Loading metadata...\n");
    try {
        meta = new Metadata(metadataPath);
    } catch (const std::exception& e) {
        fprintf(stderr, "Metadata error: %s\n", e.what()); return 1;
    } catch (...) {
        fprintf(stderr, "Metadata unknown error\n"); return 1;
    }
    printf("Metadata version: %.1f\n", meta->version);

    fprintf(stderr, "DEBUG: Loading ELF...\n");
    try {
        il = openElf(elfPath);
    } catch (const std::exception& e) {
        fprintf(stderr, "ELF error: %s\n", e.what());
        delete meta; return 1;
    } catch (...) {
        fprintf(stderr, "ELF unknown error\n"); delete meta; return 1;
    }
    printf("ELF loaded: %s\n", il->is32bit ? "32-bit" : "64-bit");

    if (!tryInit(il, meta, forceCode, forceMeta)) {
        delete il; delete meta; return 1;
    }

    Il2CppExecutor executor(meta, il);
    Dumper dumper(&executor);
    dumper.dump(cfg, outDir);

    delete il;
    delete meta;
    return 0;
}
