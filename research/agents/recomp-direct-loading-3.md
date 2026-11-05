# Recompiled direct data loading investigation — Super Dragon Ball Z

Investigation of how to load game data directly, bypassing INFO.DAT/GAME.DAT archives, in the recompiled game. Cross-referenced with native C++ host source files. Read-only analysis; no files in `ps2-web-recomp` were modified.

## Source references

- `C:\Dev\ps2-web-recomp\host\main.cpp` — ps2run host main: command-line parsing, ELF loading, emulation boot
- `C:\Dev\ps2-web-recomp\host\game_main.cpp` — SDBZ.exe host main: module path resolution, ELF discovery, windowed emulation
- `C:\Dev\ps2-web-recomp\host\web.cpp` — Emscripten web host: HTTP Range-based disc reading
- `C:\Dev\ps2-web-recomp\host\window.cpp` — SDL window/input handling
- `C:\Dev\ps2-web-recomp\sdbz\generated\SuperDragonBallZ\` — generated MIPS instruction emulator units

## Native C++ host file loading

### host/main.cpp (ps2run)

Command-line arguments (lines 25-93):
```
usage: ps2run <elf> [--arg text]... [--frames n] [--run n] [--ntsc] [--misses file]
              [--disc image.iso|folder] [--memcard card.ps2] [--pal | --ntsc] [--trace-syscalls] [--trace-io] [--trace-threads]
              [--trace-iop] [--trace-iop-hle] [--frames-dir dir] [--frame-every n]
              [--press frame:button[+button][:frames]]... [--window] [--fullscreen]
              [--save-state frame:file] [--load-state file] [--widescreen] [--patches file]
```

Key options:
- `--disc image.iso|folder` (line 61-64): Sets `ps2::g_options.disc_path` — the disc/image folder containing INFO.DAT/GAME.DAT
- `--window` / `--fullscreen` (lines 80-82): Windowed mode
- `--load-state file` (line 91): Load save state
- If no `--disc` provided (lines 95-100): Uses the ELF file's parent folder as disc path

ELF loading (lines 110-118):
```cpp
std::uint32_t entry = 0;
if (!ps2::load_elf(argv[1], entry)) { ... }
ps2::boot(entry, args);
```

### host/game_main.cpp (SDBZ.exe)

Game-specific launcher (lines 20-62):
- Uses `GetModuleFileNameW()` to locate SDBZ.exe
- Sets current directory to the EXE's folder
- Finds ELF via `ps2::game::find_elf(folder)`
- Sets `PS2_CONFIG` env var to `ps2run.ini`
- Sets `ps2::g_options.disc_path = folder.string()`
- Loads ELF and runs emulation in a window

No command-line arguments are parsed — the game folder is auto-detected from the EXE location.

### host/web.cpp (Emscripten web)

Disc reading via HTTP Range requests (lines 25-45):
```cpp
EM_JS(void, ps2web_fetch, (const char *url, double offset, int size, void *out, ...), {
    const target = UTF8ToString(url);
    fetch(target, {headers: {Range: 'bytes=' + offset + '-' + (offset + size - 1)}})
        .then(response => response.arrayBuffer())
        ...
});
```

Full file read (lines 70-83):
```cpp
EM_JS(double, ps2web_url_read, (const char *url, std::uint8_t *out, double size), {
    xhr.open('GET', UTF8ToString(url), false);
    xhr.responseType = 'arraybuffer';
    xhr.send();
    ...
});
```

Disc URL from `Module.discUrl` (line 84).

## Generated units — NO direct file loading

The generated MIPS instruction emulator units contain **zero** file I/O, path handling, or command-line argument code. They are pure instruction emulators — each unit emulates MIPS CPU instructions at a specific VA range, and file access happens through MIPS syscalls that are intercepted and handled by the host layer.

| Search term | Matches | Notes |
|---|---|---|
| fopen, fread, fseek, fclose, FILE | 0 | No C stdio file I/O |
| argv, argc, main | 0 | No command-line argument processing |
| path, PATH, filename, FILENAME, directory, DIRECTORY | 0 | No path/filename string handling |
| override, external, direct, bypass | 0 | No archive bypass mechanisms |
| dev_mode, test_mode | 0 | No debug/development modes |
| debug, DEBUG | 0 | No debug mode strings |
| syscall (MIPS instruction) | 100+ | MIPS syscall instructions — handled by host layer |

Syscall examples (unit_001D.cpp, lines 699-1147):
```cpp
c.pc = 0x001746A8u; syscall(c, 0x001746A4u); if (c.pc != 0x001746A8u) return;
c.pc = 0x001746C8u; syscall(c, 0x001746C4u); if (c.pc != 0x001746C8u) return;
```

These are MIPS `syscall` instructions at specific VAs — the actual syscall handler is in the host's ps2/system layer, not in the generated units.

## How data loading works in the recompiled game

```
Host layer (main.cpp / game_main.cpp / web.cpp)
├── Parses --disc argument or auto-detects ELF folder
├── Loads ELF via ps2::load_elf()
├── Boots emulation via ps2::boot()
└── Handles MIPS syscalls for file I/O
    ├── web.cpp: HTTP Range/fetch() requests for disc data
    └── Native: standard file I/O via ps2::disc layer

Generated units (unit_0000.cpp - unit_00DF.cpp)
└── Pure MIPS instruction emulation
    ├── Executes CPU instructions
    ├── Issues syscall(c, addr) for OS services
    └── No file I/O, path handling, or archive logic
```

## Archive bypass / direct loading — NOT present in generated units

No mechanism exists in the generated units to:
- Bypass INFO.DAT/GAME.DAT archives
- Load files directly by name from the filesystem
- Override archive paths with external directories
- Enable debug/dev/test modes that expose raw file loading

All file access goes through the MIPS syscall interface, which the host layer translates to:
- Native file I/O (ps2run/SDBZ.exe)
- HTTP Range requests (web build)

## Implications for direct file loading

To bypass INFO.DAT/GAME.DAT and load game data directly:

1. **Host layer modification**: Modify `host/main.cpp` or `host/game_main.cpp` to intercept disc path resolution and load files from an external directory instead of the archive. The `--disc` argument already accepts a folder path.

2. **Syscall interception**: The MIPS `syscall` instructions in generated units (e.g., unit_001D.cpp:699, unit_001F.cpp:2942, unit_00DA.cpp:232) are the entry points for file operations. Intercepting these at the host level would allow redirecting file reads.

3. **No generated unit changes needed**: Since the generated units contain no file loading logic, no modifications to `unit_*.cpp` files are required for a direct-loading bypass. All changes would be in the host layer.

4. **Web build**: `host/web.cpp` uses `ps2web_fetch()` with HTTP Range requests — direct loading would require serving individual files instead of a disc image.

## ELF address mapping

The generated units use their own VA ranges (e.g., unit_001D.cpp contains syscalls at 0x001746A4–0x00174F14, unit_00DA.cpp at 0x00468510–0x0046B9A4). These VAs do not directly correspond to ELF file offsets — they are runtime virtual addresses after ELF loading. Mapping syscall VAs to specific file operations would require:
1. Cross-referencing with the original ELF binary (SLES_541.61)
2. Identifying which syscalls perform file I/O vs. other OS services
3. Tracing the syscall handler in the host's ps2/system layer
