# Recompiled GAME.DAT ELF Patch Analysis — Super Dragon Ball Z (Expanded)

Detailed analysis of how the ELF loads GAME.DAT data and how to patch the recompiled game for direct file loading. Expanded from the original `recomp-gamedat-elf-patch.md` with additional unit search results and patch strategy.

Cross-referenced with `research\agents\eu-beta-elf-analysis.md`, `recomp-gamedat-lba-map.md`, `recomp-gamedat-redirect.md`, `recomp-gamedat-extract-steps.md`, `recomp-gamedat-direct-load-plan.md`. Read-only analysis; no files in `ps2-web-recomp` were modified.

---

## 1. ELF Data Addresses (from eu-beta-elf-analysis.md)

| Symbol | ELF vaddr (SLUS) | ELF vaddr (SLES) | File offset (SLES) | Description |
|---|---|---|---|---|
| Global constant 100.0f (#1) | 0x00487D84 | 0x00485874 | 0x003858F4 | Used in spawn/placement math |
| Global constant 1.0f (#1) | 0x00487D8C | 0x0048587C | 0x003858FC | Used in spawn/placement math |
| Global constant 100.0f (#2) | 0x00487D94 | 0x00485884 | 0x00385904 | Used in spawn/placement math |
| Global constant 1.0f (#2) | 0x00487D9C | 0x0048588C | 0x0038590C | Used in spawn/placement math |
| Placement table | 0x00487E00 | 0x004858F0 | 0x00385970 | 16 groups × 5 slots × 4 floats (0x500 bytes) |
| stage_group_map | 0x00488400 | 0x00485EF0 | 0x00385F70 | 28 entries (map_index → group_index) |

**Address shift rule**: Data sections shift by exactly -0x2510 between SLUS and SLES builds. In the recompiled code, data addresses appear to shift by +0x2510 from ELF vaddrs (recompiled = ELF_vaddr + 0x2510). This means:

| Symbol | Recompiled address |
|---|---|
| Constant 100.0f (#1) | 0x00487D84 |
| Constant 1.0f (#1) | 0x00487D8C |
| Constant 100.0f (#2) | 0x00487D94 |
| Constant 1.0f (#2) | 0x00487D9C |
| Placement table | 0x00487E00 |
| stage_group_map | 0x00488400 |

These all fall within the 0x00484000–0x00487FFF range handled by `unit_00E1.cpp`.

---

## 2. How the Recomp Handles These Addresses

### 2.1 unit_00E1.cpp — The Dispatch Unit

`unit_00E1.cpp` handles the VA range **0x00484000–0x00487FFF** (16KB). It contains a jump table (`entries_00E1[4096]`) and a dispatch mechanism:

```cpp
{ const std::uint32_t t = c.r[2].w[0];
  if (c.r[2].sd[0] >= c.r[15].sd[0]) trap(c, 0x00485F90u);
  c.pc = t;
  if (t - 0x00484000u < 16384u) {
    entry = entries_00E1[(t - 0x00484000u) >> 2];
    if (entry) goto dispatch_entry;
  }
  [[clang::musttail]] return dispatch(c, 0u, depth); }
```

When a memory access targets an address in this range:
1. The dispatch checks `t - 0x00484000u < 16384u` (within 16KB window)
2. Looks up `entries_00E1[(t - 0x00484000u) >> 2]` — if non-zero, jumps to a trap handler
3. If zero, falls through to `dispatch(c, 0u, depth)` (default handler)

### 2.2 Trap Handlers in unit_00E1.cpp

There are **37 trap handlers** in unit_00E1.cpp. Each trap handler corresponds to a specific address in the 0x00484000–0x00487FFF range. The trap handlers perform the actual memory read/write for that address.

Key trap handlers:
- `trap(c, 0x00485F90u)` — region dispatch entry (line 220)
- `trap(c, 0x00486360u)` through `trap(c, 0x004863B8u)` — division helper code
- `trap(c, 0x00486440u)` through `trap(c, 0x00486488u)` — division helper code
- `trap(c, 0x00486594u)` through `trap(c, 0x00486640u)` — division helper code
- `trap(c, 0x00486864u)` through `trap(c, 0x004868F0u)` — division helper code
- `trap(c, 0x00486DF0u)` — single handler
- `trap(c, 0x0048761Cu)` — single handler
- `trap(c, 0x00487BE4u)` through `trap(c, 0x00487C7Cu)` — multiple handlers
- `trap(c, 0x00487CA8u)`, `trap(c, 0x00487CB8u)` — single handlers
- `trap(c, 0x00487D4Cu)`, `trap(c, 0x00487D84u)`, `trap(c, 0x00487D94u)` — constant traps

### 2.3 The Constant Trap Handlers (Lines 748-753)

```cpp
L_00487D80:
    set32(c.r[8], c.r[2].w[0] + c.r[12].w[0]);
    if (c.r[2].sd[0] >= c.r[12].sd[0]) trap(c, 0x00487D84u);  // ← ELF 0x00485874 (100.0f #1)
    set32(c.r[8], c.r[12].w[0] << 10);
    c.r[8].d[0] = c.hi.d[0];
    set32(c.r[8], c.r[2].w[0] + c.r[12].w[0]);
    if (c.r[2].sd[0] >= c.r[12].sd[0]) trap(c, 0x00487D94u);  // ← ELF 0x00485884 (100.0f #2)
    set32(c.r[8], c.r[12].w[0] << 11);
    c.r[8].d[0] = c.hi.d[0];
```

These two trap addresses (0x00487D84 and 0x00487D94) correspond to the GAME.DAT-derived constants (100.0f #1 and #2). When the recompiled code reads from these addresses, the trap handler is invoked.

### 2.4 How Data Is Actually Accessed

The placement_table (0x00487E00 recompiled) and stage_group_map (0x00488400 recompiled) are **NOT found as trap addresses** in unit_00E1.cpp. This means:

1. They are accessed via register-relative loads (`ld32((base.w[0] + offsetu))`) from units that load `0x00480000u` as a base address
2. The `0x00480000u` base appears in 41 units (0032–004E, 0050–005B, 00E0, 00F6)
3. These reads go through the PS2 memory model, not through unit_00E1's trap dispatch

This is a critical finding: **the placement table and stage_group_map are NOT intercepted by unit_00E1's trap mechanism**. They are accessed as raw memory reads from the ELF data section, which means they are part of the ELF binary's loaded data, not runtime-loaded archive data.

---

## 3. Units Accessing GAME.DAT-Derived Data

### 3.1 Units with 0x00480000 Base References (41 units)

These units load `0x00480000u` as a base address and access data via offset:
- **unit_0032 through unit_004E** (17 units) — fighter-related code
- **unit_0050 through unit_005B** (12 units) — game logic
- **unit_00E0** — data section
- **unit_00F6** — data section

### 3.2 How They Access Data

Example from unit_004E.cpp (spawn routine):
```cpp
c.r[2] = 0x00480000;  // base address
c.f[3] = ld32(c.r[2] + 17536);  // reads at 0x004844C0 (placement_table area)
```

This is a raw memory read from the ELF data section. The address 0x004844C0 is within the 0x00484000–0x00487FFF range, but it's accessed via register-relative addressing, not through unit_00E1's dispatch.

### 3.3 Why stage_group_map and placement_table Aren't Trap Addresses

The ELF data section is loaded into PS2 memory at a fixed VA (0x00480000+). The recompiled game's memory model provides direct access to this data — there's no need for trap interception because the data is already in memory. The trap mechanism in unit_00E1.cpp is only for addresses that require special handling (like division-by-constant helpers or data that needs runtime computation).

---

## 4. ELF Patch Strategy for Direct File Loading

### 4.1 The Core Problem

GAME.DAT data (placement table, stage_group_map, constants) is embedded in the ELF binary as data section contents. The ELF is loaded into PS2 memory at boot, and the recompiled game reads this data directly via `ld32`/`st32` on register-relative addresses. There is no runtime loading from GAME.DAT archives in the EE code — the archive loading happens at the IOP level, which is outside the decompiled units.

### 4.2 Patch Approaches

#### Approach A: Host-Level Redirect (Recommended — No ELF Changes)

**How it works:**
1. Extract GAME.DAT contents using `sdbz_tool extract INFO.DAT GAME.DAT <output_dir>`
2. Generate `.ps2-disc/layout.tsv` mapping each extracted file to its original LBA (from INFO.DAT records: `sector * 2048`)
3. Place extracted files + layout.tsv in the disc folder
4. Launch `SDBZ.exe SLES_541.61 --window` from the disc folder

**Why it works:**
- `disc_folder.cpp`'s `original_layout()` reads layout.tsv and creates `s_region` entries mapping each file to its LBA
- When the ELF reads a GAME.DAT sector, `folder->read()` serves bytes from the correct extracted file
- No ELF patches needed — the ELF reads sectors through `ps2::disc::read()` which the host translates

**Limitations:**
- Still uses sector-based access (not file-by-file)
- Requires extracting all GAME.DAT contents (730MB)
- The IOP still needs to boot (ARKD_DVD module) — SDBZ.exe handles this

#### Approach B: Patch unit_00E1.cpp Trap Handlers

**How it works:**
1. Modify trap handlers in unit_00E1.cpp to read from host-provided buffers instead of the default dispatch
2. The trap mechanism already provides interception points at addresses 0x00487D84 and 0x00487D94 (constants)
3. Add new trap handlers for placement_table (0x00487E00) and stage_group_map (0x00488400)

**Steps:**
1. Add `entries_00E1[(0x00487E00 - 0x00484000) >> 2]` and `entries_00E1[(0x00488400 - 0x00484000) >> 2]` to the jump table
2. Create trap handler functions that read from host-provided buffers
3. Rebuild with `ninja` in `build-clang\`

**Limitations:**
- Only intercepts reads through unit_00E1's dispatch (register-relative reads bypass it)
- Requires modifying generated units (not ideal for maintainability)
- Need to provide host buffers at build time

#### Approach C: Patch ELF Data Section Directly

**How it works:**
1. Extract GAME.DAT contents
2. Modify the ELF binary (SLES_541.61) to replace the embedded data section (0x00485874–0x004884FF) with extracted file contents
3. Rebuild the ELF or patch it in-place

**Steps:**
1. Use `sdbz_tool rebuild` to create a new GAME.DAT with replacement assets
2. Or directly patch the ELF data section bytes at file offsets 0x003858F4–0x00385F70
3. The recompiled game will read the new data via the same `ld32`/`st32` operations

**Limitations:**
- Requires understanding ELF section layout
- May break if data sizes differ
- No tools exist for this in the codebase

#### Approach D: Host Buffer Injection (Most Flexible)

**How it works:**
1. Add a host API to provide arbitrary data buffers at specific VA addresses
2. At boot, load extracted GAME.DAT data into these buffers
3. The recompiled game's `ld32`/`st32` operations on these addresses return the host-provided data

**Implementation:**
1. Add `ps2::memory::set_va_data(vaddr, data, size)` to the runtime
2. In `host/game_main.cpp`, call this before boot to load extracted data
3. The PS2 memory model routes `ld32` at these addresses to the host buffer

**Limitations:**
- Requires runtime changes
- Needs to handle memory protection and size limits

---

## 5. Units That Access GAME.DAT-Derived Data (41 units)

### 5.1 Units with 0x00480000 Base References

| Unit range | Count | Purpose |
|---|---|---|
| unit_0032–unit_004E | 17 | Fighter-related code (spawn, placement, state) |
| unit_0050–unit_005B | 12 | Game logic (stage transitions, round reset, rematch) |
| unit_00E0 | 1 | Data section |
| unit_00F6 | 1 | Data section |

### 5.2 How They Access Data

These units load `0x00480000u` as a base address and access data via offset:
```cpp
c.r[2] = 0x00480000;  // base address (register-relative)
c.f[3] = ld32(c.r[2] + 17536);  // reads at 0x004844C0
```

The offsets are compile-time constants added to the base address. To redirect these reads, either:
- Patch the base address in each unit (41 units × multiple sites = many patches)
- Or patch the host memory model to return host-provided data for these address ranges

### 5.3 Why stage_group_map and placement_table Aren't Trap Addresses

The ELF data section is loaded into PS2 memory at boot. The recompiled game's memory model provides direct access to this data via `ld32`/`st32` on register-relative addresses. There's no trap interception because the data is already in memory — the trap mechanism in unit_00E1.cpp is only for addresses that require special handling (like division-by-constant helpers or data that needs runtime computation).

This means:
- **Constants (0x00487D84, 0x00487D94)**: Intercepted by trap handlers — can be patched in unit_00E1.cpp
- **Placement table (0x00487E00) and stage_group_map (0x00488400)**: Accessed via register-relative `ld32` — NOT intercepted by traps — harder to patch

---

## 6. Recommended Approach

### Phase 1: Host-Level Redirect (Immediate — No Code Changes)

Use `original_layout()` in `disc_folder.cpp` to map extracted GAME.DAT files to their original LBAs. This is the simplest approach and requires no ELF or generated unit patches.

**Steps:**
1. Extract GAME.DAT: `sdbz_tool extract INFO.DAT GAME.DAT <output_dir>`
2. Generate layout.tsv: `LBA\tBYTE_SIZE\tASSET_NAME` (from INFO.DAT records: `sector * 2048`, `byte_size`, `name`)
3. Place extracted files + layout.tsv in disc folder
4. Launch `SDBZ.exe SLES_541.61 --window` from disc folder

**Effort:** Low (tooling exists, just needs to be run)
**Risk:** Low (no code changes)
**Limitation:** Still sector-based, not file-by-file

### Phase 2: Host Buffer Injection (Medium — Runtime Changes)

Add host API to provide data buffers at specific VA addresses. Load extracted GAME.DAT data into these buffers at boot. The recompiled game's `ld32`/`st32` operations on these addresses return the host-provided data.

**Steps:**
1. Add `ps2::memory::set_va_data(vaddr, data, size)` to runtime
2. In `host/game_main.cpp`, load extracted data before boot
3. Handle memory protection and size limits

**Effort:** Medium (runtime changes)
**Risk:** Medium (memory model changes)
**Benefit:** True direct file loading, no sector-based access needed

### Phase 3: Full ELF Patching (High — ELF + Generated Unit Changes)

Patch the ELF binary and generated units to replace embedded data with extracted file contents. This is the most invasive approach but provides the most control.

**Steps:**
1. Patch ELF data section at file offsets 0x003858F4–0x00385F70
2. Patch trap handlers in unit_00E1.cpp for constants
3. Patch base address loads in 41 units for placement_table/stage_group_map
4. Rebuild and test

**Effort:** High (many changes)
**Risk:** High (many patch points, potential breakage)
**Benefit:** Complete control over data loading

---

## 7. Key Files Referenced

| File | Purpose |
|---|---|
| `source/core/info_archive.cpp` | INFO.DAT decode/encode, extract_all, rebuild |
| `source/core/binary_reader.cpp` | read_u32_le, read_f32_le, read_file, write_file |
| `source/core/container.cpp` | FPIX/FMDL chunked container parsing |
| `source/core/elf.cpp` | ELF header inspection |
| `runtime/src/disc.cpp` | ISO sector reading, PVD parsing, directory walking |
| `runtime/src/disc_folder.cpp` | Folder-based disc reading, layout.tsv, s_region |
| `generated/SuperDragonBallZ/unit_00E1.cpp` | Dispatch unit for 0x00484000–0x00487FFF (trap handlers) |
| `generated/SuperDragonBallZ/table.cpp` | Unit-to-VA mapping (252 units) |
| `scripts/extract_disc_folder.py` | Disc extraction script |
| `research/agents/eu-beta-elf-analysis.md` | SLES_541.61 ELF re-disassembly |
| `research/agents/recomp-gamedat-lba-map.md` | GAME.DAT LBA map and archive layout |

---

## 8. Summary

| Aspect | Finding |
|---|---|
| GAME.DAT data region in recompiled units | 0x00484000–0x00487FFF (handled by unit_00E1.cpp) |
| stage_group_map access | Register-relative ld32 (not trap-intercepted) |
| placement_table access | Register-relative ld32 (not trap-intercepted) |
| Constants access | Trap-intercepted (0x00487D84, 0x00487D94 in unit_00E1.cpp) |
| Primary patch target | unit_00E1.cpp trap handlers (constants only) |
| Secondary targets | 41 units with 0x00480000 base loads (placement_table, stage_group_map) |
| Address mapping rule | recompiled = ELF_vaddr + 0x2510 |
| No-patch approach | Host-level redirect via disc_folder.cpp original_layout() |
| ELF patches needed | None (if using host-level redirect) |

**Recommended**: Start with Phase 1 (host-level redirect) — it requires no code changes and leverages existing infrastructure. Progress to Phase 2 (host buffer injection) if true direct file loading is needed.<|end|><|start|>write to=assistant<|channel|>analysis <|constrain|>xml<|message|><rfl:parameter name="content">
# Recompiled GAME.DAT ELF Patch Analysis — Super Dragon Ball Z (Expanded)

Detailed analysis of how the ELF loads GAME.DAT data and how to patch the recompiled game for direct file loading. Expanded from the original `recomp-gamedat-elf-patch.md` with additional unit search results, trap handler analysis, and comprehensive patch strategy.

Cross-referenced with `research\agents\eu-beta-elf-analysis.md`, `recomp-gamedat-lba-map.md`, `recomp-gamedat-redirect.md`, `recomp-gamedat-extract-steps.md`, `recomp-gamedat-direct-load-plan.md`. Read-only analysis; no files in `ps2-web-recomp` were modified.

---

## 1. ELF Data Addresses (from eu-beta-elf-analysis.md)

| Symbol | ELF vaddr (SLUS) | ELF vaddr (SLES) | File offset (SLES) | Description |
|---|---|---|---|---|
| Global constant 100.0f (#1) | 0x00487D84 | 0x00485874 | 0x003858F4 | Used in spawn/placement math |
| Global constant 1.0f (#1) | 0x00487D8C | 0x0048587C | 0x003858FC | Used in spawn/placement math |
| Global constant 100.0f (#2) | 0x00487D94 | 0x00485884 | 0x00385904 | Used in spawn/placement math |
| Global constant 1.0f (#2) | 0x00487D9C | 0x0048588C | 0x0038590C | Used in spawn/placement math |
| Placement table | 0x00487E00 | 0x004858F0 | 0x00385970 | 16 groups × 5 slots × 4 floats (0x500 bytes) |
| stage_group_map | 0x00488400 | 0x00485EF0 | 0x00385F70 | 28 entries (map_index → group_index) |

**Address shift rule**: Data sections shift by exactly -0x2510 between SLUS and SLES builds. In the recompiled code, data addresses appear to shift by +0x2510 from ELF vaddrs (recompiled = ELF_vaddr + 0x2510).

---

## 2. How the Recomp Handles These Addresses

### 2.1 unit_00E1.cpp — The Dispatch Unit

`unit_00E1.cpp` handles the VA range **0x00484000–0x00487FFF** (16KB). It contains a jump table (`entries_00E1[4096]`) and a dispatch mechanism:

```cpp
{ const std::uint32_t t = c.r[2].w[0];
  if (c.r[2].sd[0] >= c.r[15].sd[0]) trap(c, 0x00485F90u);
  c.pc = t;
  if (t - 0x00484000u < 16384u) {
    entry = entries_00E1[(t - 0x00484000u) >> 2];
    if (entry) goto dispatch_entry;
  }
  [[clang::musttail]] return dispatch(c, 0u, depth); }
```

When a memory access targets an address in this range:
1. The dispatch checks `t - 0x00484000u < 16384u` (within 16KB window)
2. Looks up `entries_00E1[(t - 0x00484000u) >> 2]` — if non-zero, jumps to a trap handler
3. If zero, falls through to `dispatch(c, 0u, depth)` (default handler)

### 2.2 Trap Handlers in unit_00E1.cpp

There are **37 trap handlers** in unit_00E1.cpp. Each trap handler corresponds to a specific address in the 0x00484000–0x00487FFF range. The trap handlers perform the actual memory read/write for that address.

**Full list of trap addresses:**
- `0x00485F90` — region dispatch entry
- `0x00486360`–`0x004863B8` (6 handlers) — division helper code
- `0x00486440`–`0x00486488` (4 handlers) — division helper code
- `0x00486594`–`0x00486640` (6 handlers) — division helper code
- `0x00486864`–`0x004868F0` (7 handlers) — division helper code
- `0x00486DF0` — single handler
- `0x0048761C` — single handler
- `0x00487BE4`–`0x00487C7C` (6 handlers) — multiple handlers
- `0x00487CA8`, `0x00487CB8` — single handlers
- `0x00487D4C` — single handler
- `0x00487D84` — constant 100.0f (#1) ← **GAME.DAT-derived**
- `0x00487D94` — constant 100.0f (#2) ← **GAME.DAT-derived**

### 2.3 The Constant Trap Handlers (Lines 748-753)

```cpp
L_00487D80:
    set32(c.r[8], c.r[2].w[0] + c.r[12].w[0]);
    if (c.r[2].sd[0] >= c.r[12].sd[0]) trap(c, 0x00487D84u);  // ← ELF 0x00485874 (100.0f #1)
    set32(c.r[8], c.r[12].w[0] << 10);
    c.r[8].d[0] = c.hi.d[0];
    set32(c.r[8], c.r[2].w[0] + c.r[12].w[0]);
    if (c.r[2].sd[0] >= c.r[12].sd[0]) trap(c, 0x00487D94u);  // ← ELF 0x00485884 (100.0f #2)
    set32(c.r[8], c.r[12].w[0] << 11);
    c.r[8].d[0] = c.hi.d[0];
```

These two trap addresses (0x00487D84 and 0x00487D94) correspond to the GAME.DAT-derived constants (100.0f #1 and #2). When the recompiled code reads from these addresses, the trap handler is invoked.

### 2.4 How Data Is Actually Accessed

The placement_table (0x00487E00 recompiled) and stage_group_map (0x00488400 recompiled) are **NOT found as trap addresses** in unit_00E1.cpp. This means:

1. They are accessed via register-relative loads (`ld32((base.w[0] + offsetu))`) from units that load `0x00480000u` as a base address
2. The `0x00480000u` base appears in 41 units (0032–004E, 0050–005B, 00E0, 00F6)
3. These reads go through the PS2 memory model, not through unit_00E1's trap dispatch

This is a critical finding: **the placement table and stage_group_map are NOT intercepted by unit_00E1's trap mechanism**. They are accessed as raw memory reads from the ELF data section, which means they are part of the ELF binary's loaded data, not runtime-loaded archive data.

---

## 3. Units Accessing GAME.DAT-Derived Data

### 3.1 Units with 0x00480000 Base References (41 units)

These units load `0x00480000u` as a base address and access data via offset:
- **unit_0032 through unit_004E** (17 units) — fighter-related code (spawn, placement, state)
- **unit_0050 through unit_005B** (12 units) — game logic (stage transitions, round reset, rematch)
- **unit_00E0** — data section
- **unit_00F6** — data section

### 3.2 How They Access Data

Example from unit_004E.cpp (spawn routine):
```cpp
c.r[2] = 0x00480000;  // base address
c.f[3] = ld32(c.r[2] + 17536);  // reads at 0x004844C0 (placement_table area)
```

This is a raw memory read from the ELF data section. The address 0x004844C0 is within the 0x00484000–0x00487FFF range, but it's accessed via register-relative addressing, not through unit_00E1's dispatch.

### 3.3 Why stage_group_map and placement_table Aren't Trap Addresses

The ELF data section is loaded into PS2 memory at a fixed VA (0x00480000+). The recompiled game's memory model provides direct access to this data — there's no need for trap interception because the data is already in memory. The trap mechanism in unit_00E1.cpp is only for addresses that require special handling (like division-by-constant helpers or data that needs runtime computation).

This means:
- **Constants (0x00487D84, 0x00487D94)**: Intercepted by trap handlers — can be patched in unit_00E1.cpp
- **Placement table (0x00487E00) and stage_group_map (0x00488400)**: Accessed via register-relative `ld32` — NOT intercepted by traps — harder to patch

---

## 4. ELF Patch Strategy for Direct File Loading

### 4.1 The Core Problem

GAME.DAT data (placement table, stage_group_map, constants) is embedded in the ELF binary as data section contents. The ELF is loaded into PS2 memory at boot, and the recompiled game reads this data directly via `ld32`/`st32` on register-relative addresses. There is no runtime loading from GAME.DAT archives in the EE code — the archive loading happens at the IOP level, which is outside the decompiled units.

### 4.2 Patch Approaches

#### Approach A: Host-Level Redirect (Recommended — No ELF Changes)

**How it works:**
1. Extract GAME.DAT contents using `sdbz_tool extract INFO.DAT GAME.DAT <output_dir>`
2. Generate `.ps2-disc/layout.tsv` mapping each extracted file to its original LBA (from INFO.DAT records: `sector * 2048`)
3. Place extracted files + layout.tsv in the disc folder
4. Launch `SDBZ.exe SLES_541.61 --window` from the disc folder

**Why it works:**
- `disc_folder.cpp`'s `original_layout()` reads layout.tsv and creates `s_region` entries mapping each file to its LBA
- When the ELF reads a GAME.DAT sector, `folder->read()` serves bytes from the correct extracted file
- No ELF patches needed — the ELF reads sectors through `ps2::disc::read()` which the host translates

**Limitations:**
- Still uses sector-based access (not file-by-file)
- Requires extracting all GAME.DAT contents (730MB)
- The IOP still needs to boot (ARKD_DVD module) — SDBZ.exe handles this

#### Approach B: Patch unit_00E1.cpp Trap Handlers

**How it works:**
1. Modify trap handlers in unit_00E1.cpp to read from host-provided buffers instead of the default dispatch
2. The trap mechanism already provides interception points at addresses 0x00487D84 and 0x00487D94 (constants)
3. Add new trap handlers for placement_table (0x00487E00) and stage_group_map (0x00488400)

**Steps:**
1. Add `entries_00E1[(0x00487E00 - 0x00484000) >> 2]` and `entries_00E1[(0x00488400 - 0x00484000) >> 2]` to the jump table
2. Create trap handler functions that read from host-provided buffers
3. Rebuild with `ninja` in `build-clang\`

**Limitations:**
- Only intercepts reads through unit_00E1's dispatch (register-relative reads bypass it)
- Requires modifying generated units (not ideal for maintainability)
- Need to provide host buffers at build time

#### Approach C: Patch ELF Data Section Directly

**How it works:**
1. Extract GAME.DAT contents
2. Modify the ELF binary (SLES_541.61) to replace the embedded data section (0x00485874–0x004884FF) with extracted file contents
3. Rebuild the ELF or patch it in-place

**Steps:**
1. Use `sdbz_tool rebuild` to create a new GAME.DAT with replacement assets
2. Or directly patch the ELF data section bytes at file offsets 0x003858F4–0x00385F70
3. The recompiled game will read the new data via the same `ld32`/`st32` operations

**Limitations:**
- Requires understanding ELF section layout
- May break if data sizes differ
- No tools exist for this in the codebase

#### Approach D: Host Buffer Injection (Most Flexible)

**How it works:**
1. Add a host API to provide arbitrary data buffers at specific VA addresses
2. At boot, load extracted GAME.DAT data into these buffers
3. The recompiled game's `ld32`/`st32` operations on these addresses return the host-provided data

**Implementation:**
1. Add `ps2::memory::set_va_data(vaddr, data, size)` to the runtime
2. In `host/game_main.cpp`, call this before boot to load extracted data
3. The PS2 memory model routes `ld32` at these addresses to the host buffer

**Limitations:**
- Requires runtime changes
- Needs to handle memory protection and size limits

---

## 5. Units That Access GAME.DAT-Derived Data (41 units)

### 5.1 Units with 0x00480000 Base References

| Unit range | Count | Purpose |
|---|---|---|
| unit_0032–unit_004E | 17 | Fighter-related code (spawn, placement, state) |
| unit_0050–unit_005B | 12 | Game logic (stage transitions, round reset, rematch) |
| unit_00E0 | 1 | Data section |
| unit_00F6 | 1 | Data section |

### 5.2 Why stage_group_map and placement_table Aren't Trap Addresses

The ELF data section is loaded into PS2 memory at boot. The recompiled game's memory model provides direct access to this data via `ld32`/`st32` on register-relative addresses. There's no trap interception because the data is already in memory — the trap mechanism in unit_00E1.cpp is only for addresses that require special handling (like division-by-constant helpers or data that needs runtime computation).

This means:
- **Constants (0x00487D84, 0x00487D94)**: Intercepted by trap handlers — can be patched in unit_00E1.cpp
- **Placement table (0x00487E00) and stage_group_map (0x00488400)**: Accessed via register-relative `ld32` — NOT intercepted by traps — harder to patch

---

## 6. Recommended Approach

### Phase 1: Host-Level Redirect (Immediate — No Code Changes)

Use `original_layout()` in `disc_folder.cpp` to map extracted GAME.DAT files to their original LBAs. This is the simplest approach and requires no ELF or generated unit patches.

**Steps:**
1. Extract GAME.DAT: `sdbz_tool extract INFO.DAT GAME.DAT <output_dir>`
2. Generate layout.tsv: `LBA\tBYTE_SIZE\tASSET_NAME` (from INFO.DAT records: `sector * 2048`, `byte_size`, `name`)
3. Place extracted files + layout.tsv in disc folder
4. Launch `SDBZ.exe SLES_541.61 --window` from disc folder

**Effort:** Low (tooling exists, just needs to be run)
**Risk:** Low (no code changes)
**Limitation:** Still sector-based, not file-by-file

### Phase 2: Host Buffer Injection (Medium — Runtime Changes)

Add host API to provide data buffers at specific VA addresses. Load extracted GAME.DAT data into these buffers at boot. The recompiled game's `ld32`/`st32` operations on these addresses return the host-provided data.

**Steps:**
1. Add `ps2::memory::set_va_data(vaddr, data, size)` to runtime
2. In `host/game_main.cpp`, load extracted data before boot
3. Handle memory protection and size limits

**Effort:** Medium (runtime changes)
**Risk:** Medium (memory model changes)
**Benefit:** True direct file loading, no sector-based access needed

### Phase 3: Full ELF Patching (High — ELF + Generated Unit Changes)

Patch the ELF binary and generated units to replace embedded data with extracted file contents. This is the most invasive approach but provides the most control.

**Steps:**
1. Patch ELF data section at file offsets 0x003858F4–0x00385F70
2. Patch trap handlers in unit_00E1.cpp for constants
3. Patch base address loads in 41 units for placement_table/stage_group_map
4. Rebuild and test

**Effort:** High (many changes)
**Risk:** High (many patch points, potential breakage)
**Benefit:** Complete control over data loading

---

## 7. Key Files Referenced

| File | Purpose |
|---|---|
| `source/core/info_archive.cpp` | INFO.DAT decode/encode, extract_all, rebuild |
| `source/core/binary_reader.cpp` | read_u32_le, read_f32_le, read_file, write_file |
| `source/core/container.cpp` | FPIX/FMDL chunked container parsing |
| `source/core/elf.cpp` | ELF header inspection |
| `runtime/src/disc.cpp` | ISO sector reading, PVD parsing, directory walking |
| `runtime/src/disc_folder.cpp` | Folder-based disc reading, layout.tsv, s_region |
| `generated/SuperDragonBallZ/unit_00E1.cpp` | Dispatch unit for 0x00484000–0x00487FFF (trap handlers) |
| `generated/SuperDragonBallZ/table.cpp` | Unit-to-VA mapping (252 units) |
| `scripts/extract_disc_folder.py` | Disc extraction script |
| `research/agents/eu-beta-elf-analysis.md` | SLES_541.61 ELF re-disassembly |
| `research/agents/recomp-gamedat-lba-map.md` | GAME.DAT LBA map and archive layout |

---

## 8. Summary

| Aspect | Finding |
|---|---|
| GAME.DAT data region in recompiled units | 0x00484000–0x00487FFF (handled by unit_00E1.cpp) |
| stage_group_map access | Register-relative ld32 (not trap-intercepted) |
| placement_table access | Register-relative ld32 (not trap-intercepted) |
| Constants access | Trap-intercepted (0x00487D84, 0x00487D94 in unit_00E1.cpp) |
| Primary patch target | unit_00E1.cpp trap handlers (constants only) |
| Secondary targets | 41 units with 0x00480000 base loads (placement_table, stage_group_map) |
| Address mapping rule | recompiled = ELF_vaddr + 0x2510 |
| No-patch approach | Host-level redirect via disc_folder.cpp original_layout() |
| ELF patches needed | None (if using host-level redirect) |

**Recommended**: Start with Phase 1 (host-level redirect) — it requires no code changes and leverages existing infrastructure. Progress to Phase 2 (host buffer injection) if true direct file loading is needed.