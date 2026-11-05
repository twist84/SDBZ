# Recomp Direct File Loading Investigation

## Search scope

Searched ALL generated `.cpp` files under `C:\Dev\ps2-web-recomp\sdbz\generated\SuperDragonBallZ\` for direct file loading code, plus host code files:

- `C:\Dev\ps2-web-recomp\host\main.cpp`
- `C:\Dev\ps2-web-recomp\host\game_main.cpp`
- `C:\Dev\ps2-web-recomp\host\web.cpp`
- `C:\Dev\ps2-web-recomp\recomp\main.cpp`
- `C:\Dev\ps2-web-recomp\runtime\include\ps2\disc.hpp`
- `C:\Dev\ps2-web-recomp\runtime\include\ps2\system.hpp`
- `C:\Dev\ps2-web-recomp\runtime\src\disc.cpp`
- `C:\Dev\ps2-web-recomp\runtime\src\disc_folder.cpp`

## Search terms used

| Category | Terms searched | Matches found |
|----------|---------------|---------------|
| Direct file loading | fopen, open_file, read_file, file_path, direct_load | **None** in generated units |
| File system primitives | stat, file_exists, file_size, file_read | **None** in generated units |
| Archive bypass | load_from_disk, external_file, override, debug_mode | **None** in generated units |
| Companion loading | companion, load_stage, load_model | **None** in generated units |
| Disc API usage | ps2::disc, disc::open, disc::read, disc::find | **None** in generated units |
| Config/INI | ps2run.ini, ps2_config, PS2_CONFIG, ini, config | **None** in generated units |
| Patches | load_patches, patches.txt, --patches | **None** in generated units |

## Key finding: No direct file loading in generated units

**No generated unit contains any direct file loading code.** The decompiled MIPS code does not contain:

1. **fopen/fread/fclose** — No C standard library file I/O calls
2. **open_file/read_file** — No custom file reading functions
3. **file_path/file_size/file_exists** — No file system query functions
4. **load_from_disk/external_file/override** — No archive bypass mechanisms
5. **load_stage/load_model/companion** — No stage/model/companion loading functions
6. **ps2::disc API calls** — No direct calls to `ps2::disc::open`, `ps2::disc::read`, or `ps2::disc::find`
7. **debug_mode** — No development/debug flags to skip archive loading

All file I/O in the generated units goes through the PS2 memory model (`ld32`/`st32` on absolute VA addresses), which is backed by the disc reader at the host level.

## File loading architecture — how it actually works

### Host-side disc mounting

**`host/main.cpp:61-64`** — `--disc` command-line option sets `ps2::g_options.disc_path`:
```cpp
else if (a == "--disc" && i + 1 < argc) {
    ps2::g_options.disc_path = argv[++i];
    disc_provided = true;
}
```

**`host/main.cpp:95-100`** — When no `--disc` is provided, uses the ELF's parent folder:
```cpp
if (!disc_provided) {
    auto folder = std::filesystem::path(argv[1]).parent_path();
    ps2::g_options.disc_path = folder.empty() ? "." : folder.string();
}
```

**`host/game_main.cpp:35`** — GUI mode uses the executable's folder:
```cpp
ps2::g_options.disc_path = folder.string();
```

**`host/web.cpp:303`** — Web mode opens the disc image:
```cpp
if (g_disc_size && !ps2::disc::open("disc", g_disc_size, read_disc, disc_ready))
```

### Disc API (`runtime/include/ps2/disc.hpp`)

```cpp
struct Entry {
    std::uint32_t sector;
    std::uint32_t size;
    bool directory;
};

using Reader = std::function<std::size_t(std::uint64_t offset, void *out, std::size_t size)>;
using Probe = std::function<bool(std::uint64_t offset, std::size_t size)>;

bool open(const std::string &path);
bool open(const std::string &name, std::uint64_t size, Reader reader, Probe probe = {});
bool is_open();
bool read(std::uint32_t sector, std::uint32_t count, void *out);
bool ready(std::uint32_t sector, std::uint32_t count);
std::optional<Entry> find(const std::string &path);
std::uint32_t layer1_start();
std::uint64_t sector_count();
```

### Disc implementation (`runtime/src/disc.cpp`)

- **`open(path)`** (line 66): Opens a disc image file or folder. For ISO images, reads the ISO 9660 PVD at sector 16 and builds a file index (`g_index`) mapping uppercased paths to `Entry`{sector, size, directory}. For folders, delegates to `open_folder()`.

- **`open(name, size, reader, probe)`** (line 96): Generic open with custom reader callback. Builds ISO 9660 index by walking directory records.

- **`read(sector, count, out)`** (line 121): Reads sectors via the reader callback, zero-fills short reads.

- **`find(raw)`** (line 133): Case-insensitive path lookup in the ISO 9660 index, strips `:` drive letters and `;1` version suffixes.

### Folder-based disc access (`runtime/src/disc_folder.cpp`)

When running from an extracted folder (no ISO), `open_folder()` creates a **synthetic ISO 9660 filesystem** in memory:

- `synthetic_layout()` (line 166): Walks the folder recursively, builds synthetic ISO 9660 directory records, PVD, and path tables. All files are assigned synthetic LBAs.
- `original_layout()` (line 261): Reads a `.ps2-disc/layout.tsv` file that maps original LBAs to extracted files, preserving original disc layout.

### Generated units — no file loading at all

The generated `.cpp` units contain **zero** file I/O calls. They access data exclusively through:
1. Register/memory operations (`ld32`, `st32`, `ld64`, `st64`, `st128`, `ld128`)
2. Absolute VA base addresses (e.g., `0x00480000` + offset) for data tables like stage_group_map and placement_table
3. Inter-unit calls (`unit_NNNN(c, entry, depth)`)

The PS2 memory model translates `ld32`/`st32` on absolute addresses into disc reads through the runtime's memory system, which is backed by `ps2::disc::read()`.

## Command-line options (host/main.cpp:37-93)

| Option | Purpose |
|--------|---------|
| `<elf>` | ELF file to load (positional) |
| `--arg text` | Pass argument to guest |
| `--frames n` | Max frames to run |
| `--run n` | Run n additional frames |
| `--ntsc`/`--pal` | TV standard |
| `--misses file` | Cache misses log |
| `--trace-syscalls` | Trace syscalls |
| `--trace-io` | Trace I/O |
| `--trace-threads` | Trace threads |
| `--trace-iop` | Trace IOP modules |
| `--trace-iop-hle` | Trace IOP HLE |
| `--disc image.iso\|folder` | Disc image or folder |
| `--memcard card.ps2` | Memory card path |
| `--frames-dir dir` | Frame output directory |
| `--frame-every n` | Frame capture interval |
| `--press frame:button` | Button press macro |
| `--axis` | Axis press macro |
| `--window`/`--fullscreen` | Window mode |
| `--save-state frame:file` | Save state |
| `--load-state file` | Load state |
| `--patches file` | Apply patches |
| `--widescreen` | Widescreen mode |

**No options for:** archive bypass, external file override, debug mode, or direct file loading from filesystem.

## ELF address mapping

No ELF addresses were found for file loading functions in generated units, because none exist. The INFO.DAT record walker at `unit_0030.cpp:L_001C0050` (VA 0x001C0050) reads already-decoded record data from memory — it does not perform file I/O.

## Comparison with native C++ implementation

**Native C++ (`source/core/container.cpp`):**
- `parse_chunked_container()` — parses FPIX/FMDL chunked containers from already-loaded data
- No file I/O — operates on `std::span<const std::uint8_t>`

**Native C++ (`research/archive_and_container_notes.md`):**
- INFO.DAT: 98,304 bytes, obfuscated with nibble-swap + key-subtraction codec
- GAME.DAT: Sector-based layout, payloads at `sector_offset * 2048`
- `arika_dat.py` provides CLI for list/extract/decode-info/encode-info/repack

**Generated recompiled code:**
- File loading goes through `ps2::disc::` API (ISO 9660 on disc image or extracted folder)
- No direct filesystem access for individual assets (STG/HIT/MDL/PIX)
- No archive bypass or debug mode
- INFO.DAT decoding and GAME.DAT sector reading happen at the PS2 IOP level (not in EE decompiled code)

## Conclusion

**Direct file loading from the filesystem (bypassing INFO.DAT/GAME.DAT archives) is NOT implemented in the generated recompiled code.** All file access goes through the `ps2::disc::` API which presents an ISO 9660 filesystem view over either a disc image file or an extracted folder with synthetic ISO metadata. The generated units themselves contain no file I/O — they access data through the PS2 memory model (`ld32`/`st32` on absolute addresses), which is backed by the disc reader.

To implement direct file loading, one would need to either:
1. Modify the host code (`host/main.cpp`, `host/web.cpp`) to extract assets from INFO.DAT/GAME.DAT to the filesystem before boot, then modify `disc_folder.cpp` to serve them as individual files
2. Add new `ps2::disc` reader implementations that read from extracted asset files instead of the disc image
3. Add generated unit code that calls `ps2::disc::find()` + `ps2::disc::read()` directly (currently no units do this)

No files were modified. Read-only analysis only.
