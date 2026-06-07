# CODM Dumper

IL2CPP Dumper for Call of Duty Mobile (Unity 2019.4, metadata v23).

## Requirements

- `libcodmdumper.so` — the dumper executable
- `libunity.so` — the game's native library (32-bit or 64-bit)
- `global-metadata.dat` — the game's il2cpp metadata file

---

## Usage — Normal Library (from APK)

Copy all files to `/storage/emulated/0/`, then in Termux:

```bash
cp /sdcard/libcodmdumper.so ~/
cd ~
chmod +x libcodmdumper.so
./libcodmdumper.so /sdcard/libunity.so /sdcard/global-metadata.dat /sdcard
```

The dump will be saved to `/sdcard/dump.cs`.

### Manual Addresses

If auto-detection fails, provide CodeRegistration and MetadataRegistration addresses manually:

```bash
./libcodmdumper.so libunity.so global-metadata.dat /sdcard \
    --code-reg d2463f8 --meta-reg d2464a0
```

---

## Usage — Runtime Dumped Library (Game Guardian)

For 64-bit CODM, PC-based dumpers work best with runtime-dumped libraries.
Use the included Lua script `CODM_Runtime_Dumper.lua` in Game Guardian to dump
`libunity.so` and `global-metadata.dat` from the game's memory at runtime.

After dumping, run:

```bash
./libcodmdumper.so /sdcard/libunity_dump.so /sdcard/global-metadata.dat /sdcard
```

When prompted, enter the **load address** (image base) of the dumped library
(e.g., `77ddaa4000`). You can find this in the Game Guardian memory map when
attached to the game.

The dumper will auto-detect it as a runtime dump and fix up the segment
addresses internally.

---

## Features

| Feature | Description |
|---------|-------------|
| Auto-detect | Finds CodeRegistration & MetadataRegistration automatically |
| Symbol search | Looks for `g_CodeRegistration` / `g_MetadataRegistration` symbols |
| Pattern search | ARM32 instruction patterns (32-bit) and byte patterns (all) |
| Dump detection | Automatically detects runtime-dumped libraries |
| Manual override | `--code-reg` and `--meta-reg` flags for manual addresses |
| Full dump | Fields, properties, methods, nested types, interfaces |
| Field offsets | Optional field offset output |
| TypeDefIndex | Optional TypeDefIndex annotation per class |
| Custom attributes | Dumps custom attributes per class/field/method |

---

## Lua Script (Game Guardian)

`CODM_Runtime_Dumper.lua` is provided for runtime memory dumping.
It dumps `libunity.so` and `global-metadata.dat` from the game's memory.

**How to use:**
1. Open Game Guardian
2. Attach to Call of Duty Mobile
3. Run the Lua script
4. Files are saved to `/sdcard/Download/`

---

## Output

The dumper produces `dump.cs` containing all reconstructed C# classes,
structs, enums, interfaces, fields, properties, and methods from the
il2cpp metadata.
