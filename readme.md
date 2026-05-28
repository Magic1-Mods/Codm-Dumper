# CODM Dumper Usage Guide

## 1. Copy Required Files to SD Card

Copy the following files to:

`/storage/emulated/0/`

### Required Files
- `libunity.so`
- `global-metadata.dat`
- `libcodmdumper.so`

---

## 2. Open Termux

Run the following commands:

```bash
cp /sdcard/libcodmdumper.so ~/

cd ~

chmod +x libcodmdumper.so

./libcodmdumper.so /sdcard/libunity.so /sdcard/global-metadata.dat /sdcard
```




```bash
chmod +x idadumper
./idadumper /sdcard/libgame.so
```

# for 64bit, to get a proper dump, i suggest you use the Lua script to dump
libunity.so
global-meta.dat

files at runtime