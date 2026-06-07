#pragma once
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>
#include <stdexcept>
#include <algorithm>

class BinaryStream {
public:
    std::vector<uint8_t> buf;
    size_t pos = 0;
    bool is32bit = false;
    double version = 0.0;
    uint64_t imageBase = 0;
    bool isDumped = false;

    void loadFile(const char* path) {
        FILE* f = fopen(path, "rb");
        if (!f) throw std::runtime_error(std::string("Cannot open: ") + path);
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        buf.resize((size_t)sz);
        if (fread(buf.data(), 1, (size_t)sz, f) != (size_t)sz) {
            fclose(f);
            throw std::runtime_error("Read error");
        }
        fclose(f);
        pos = 0;
    }

    uint8_t ru8() {
        if (pos >= buf.size()) throw std::out_of_range("EOF");
        return buf[pos++];
    }
    int8_t ri8() { return (int8_t)ru8(); }

    uint16_t ru16() {
        uint16_t v; memcpy(&v, buf.data()+pos, 2); pos+=2; return v;
    }
    int16_t ri16() { return (int16_t)ru16(); }

    uint32_t ru32() {
        uint32_t v; memcpy(&v, buf.data()+pos, 4); pos+=4; return v;
    }
    int32_t ri32() { return (int32_t)ru32(); }

    uint64_t ru64() {
        uint64_t v; memcpy(&v, buf.data()+pos, 8); pos+=8; return v;
    }
    int64_t ri64() { return (int64_t)ru64(); }

    uint64_t ruptr() { return is32bit ? (uint64_t)ru32() : ru64(); }
    int64_t  riptr() { return is32bit ? (int64_t)(int32_t)ru32() : ri64(); }
    uint64_t ptrSize() const { return is32bit ? 4ULL : 8ULL; }

    void seek(uint64_t p) { pos = (size_t)p; }
    uint64_t tell() const { return (uint64_t)pos; }
    uint64_t length() const { return (uint64_t)buf.size(); }

    std::string readStringToNull(uint64_t addr) {
        std::string s;
        size_t p = (size_t)addr;
        while (p < buf.size() && buf[p] != 0) s += (char)buf[p++];
        return s;
    }

    std::vector<uint8_t> readBytes(size_t count) {
        if (pos + count > buf.size()) count = buf.size() - pos;
        std::vector<uint8_t> v(buf.begin()+pos, buf.begin()+pos+count);
        pos += count;
        return v;
    }

    template<typename T>
    std::vector<T> readPODArray(uint64_t addr, size_t count) {
        pos = (size_t)addr;
        std::vector<T> v(count);
        size_t bytes = count * sizeof(T);
        if (pos + bytes > buf.size()) throw std::out_of_range("POD array OOB");
        memcpy(v.data(), buf.data()+pos, bytes);
        pos += bytes;
        return v;
    }

    std::vector<uint64_t> readPtrArray(uint64_t addr, size_t count) {
        pos = (size_t)addr;
        std::vector<uint64_t> v(count);
        for (size_t i = 0; i < count; i++) v[i] = ruptr();
        return v;
    }

    void writeU32At(size_t off, uint32_t v) { memcpy(buf.data()+off, &v, 4); }
    void writeU64At(size_t off, uint64_t v) { memcpy(buf.data()+off, &v, 8); }
    void writePtrAt(size_t off, uint64_t v) {
        if (is32bit) { uint32_t v32 = (uint32_t)v; memcpy(buf.data()+off, &v32, 4); }
        else memcpy(buf.data()+off, &v, 8);
    }
    void writeAt(size_t off, const void* data, size_t sz) {
        memcpy(buf.data()+off, data, sz);
    }

    uint32_t readCompressedU32() {
        uint8_t b = ru8();
        if ((b & 0x80) == 0) return b;
        if ((b & 0xC0) == 0x80) { uint32_t v = (b & ~0x80u) << 8; v |= ru8(); return v; }
        if ((b & 0xE0) == 0xC0) {
            uint32_t v = (b & ~0xC0u) << 24;
            v |= ((uint32_t)ru8() << 16);
            v |= ((uint32_t)ru8() << 8);
            v |= ru8();
            return v;
        }
        if (b == 0xF0) return ru32();
        if (b == 0xFE) return 0xFFFFFFFEu;
        return 0xFFFFFFFFu;
    }

    int32_t readCompressedI32() {
        uint32_t enc = readCompressedU32();
        if (enc == 0xFFFFFFFFu) return INT32_MIN;
        bool neg = (enc & 1) != 0;
        enc >>= 1;
        return neg ? -(int32_t)(enc+1) : (int32_t)enc;
    }

    // Exact Boyer-Moore-Horspool search
    std::vector<size_t> searchBytes(const uint8_t* pattern, size_t plen,
                               size_t from = 0, size_t to = SIZE_MAX) {
        std::vector<size_t> results;
        if (to == SIZE_MAX) to = buf.size();
        if (plen == 0 || plen > to - from) return results;
        int bad[256];
        for (int i = 0; i < 256; i++) bad[i] = (int)plen;
        for (size_t i = 0; i < plen-1; i++) bad[pattern[i]] = (int)(plen-1-i);
        size_t idx = from;
        while (idx <= to - plen) {
            int i = (int)plen-1;
            while (i >= 0 && buf[idx+i] == pattern[i]) i--;
            if (i < 0) results.push_back(idx);
            idx += bad[buf[idx+plen-1]];
        }
        return results;
    }

    // Wildcard pattern string search (space-delimited hex bytes, "?" for wildcard)
    // Pattern format: "0x10 ? 0xE7 0x00 ? 0xE0"
    // Supports both "0xNN" and "NN" hex formats
    std::vector<size_t> searchPattern(const std::string& patternStr,
                                      size_t from = 0, size_t to = SIZE_MAX) {
        std::vector<size_t> results;
        if (to == SIZE_MAX) to = buf.size();

        std::vector<int> pattern; // -1 for wildcard
        size_t i = 0;
        while (i < patternStr.size()) {
            if (patternStr[i] == ' ') { i++; continue; }
            if (patternStr[i] == '?') {
                pattern.push_back(-1);
                i++;
                continue;
            }
            // Parse hex byte
            size_t end = patternStr.find_first_of(" ?", i);
            if (end == std::string::npos) end = patternStr.size();
            std::string hexStr = patternStr.substr(i, end - i);
            if (hexStr.size() >= 2) {
                size_t start = (hexStr.size() > 2 && hexStr[0] == '0' && (hexStr[1] == 'x' || hexStr[1] == 'X')) ? 2 : 0;
                pattern.push_back((int)strtoul(hexStr.c_str() + start, nullptr, 16));
            }
            i = end;
        }

        size_t plen = pattern.size();
        if (plen == 0 || plen > to - from) return results;

        int bad[256];
        for (int j = 0; j < 256; j++) bad[j] = (int)plen;
        for (size_t j = 0; j < plen-1; j++)
            if (pattern[j] >= 0)
                bad[pattern[j]] = (int)(plen-1-j);

        size_t idx = from;
        while (idx <= to - plen) {
            int j = (int)plen-1;
            while (j >= 0 && (pattern[j] < 0 || buf[idx+j] == (uint8_t)pattern[j])) j--;
            if (j < 0) results.push_back(idx);
            int shift = bad[buf[idx+plen-1]];
            idx += (shift > 0) ? (size_t)shift : 1;
        }
        return results;
    }

    uint32_t readU32At(size_t off) {
        uint32_t v; memcpy(&v, buf.data()+off, 4); return v;
    }
    uint64_t readPtrAt(size_t off) {
        if (is32bit) { uint32_t v; memcpy(&v, buf.data()+off, 4); return v; }
        uint64_t v; memcpy(&v, buf.data()+off, 8); return v;
    }
};
