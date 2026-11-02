# Super Dragon Ball Z — C/C++ and SDL3 Rebuild Plan

Twister, I recommend rebuilding Super Dragon Ball Z as a native C++ game, using SDL3 for the platform layer and a custom renderer and gameplay engine.

The objective should be to recreate the original PS2 game as faithfully as possible, using the original assets, reverse-engineered formats, executable analysis and existing Arika tooling as our reference.

This should be a proper game reconstruction, not an emulator, and not a port of the existing Python or Rust editor.

[Vegeta scouter fix Mod for Super Dragon Ball Z | Super DBZ Mods](https://images.openai.com/static-rsc-4/ATnq9bF-oYmRUfjU0jwPqUOFPLLl3qilkW4x5rcREygaiR1_OnFnSuuftMsqz_r-KNMkLQkQ2pV7I4VgIRgcxmR7aYdg_a58jER6aLepvHVfJceDjr2xOgVrahQpQaHfj00phfTfycQVSLpRzsNeMvvB_u1ZAWkePwlTcgrGx6g?purpose=inline)

[Super Dragon Ball Z (2005) - MobyGames](https://images.openai.com/static-rsc-4/wFoN7itvykx6a6U6xKlvC77RVM3HL7QRBSVnsZa2XRBkiTUeOudK6RBNp-uZDEUWMaEv1w9VuDmgsz7y4G9Wx47HzQ1q62pxHZiPq4Ph6pq8PV1EHsxSC-7J_kemX0VXM81WvMo5fL1VMULNV6k6KxHuZuDMJUSTkkKlqRABcQo?purpose=inline)

[Super Dragon Ball Z for PlayStation 2](https://images.openai.com/static-rsc-4/5KzTBI6vqzyuD6JSDJJMccthh6NuRlmIXcxMnnEl7kQ2FORMGsLSfVj9CqxNNyJusQAQpfWeLvpsioQto1PMjW5Wqk6QkL35n9E51SrWE2auqd5zj0wMB5B3TikdFPFup8O-Wjhj2eqoGszfwH7KtWiZRseetHw7-wN-_XWx5-I?purpose=inline)

5

Target: Native C++

SDL3

PS2 asset compatibility

## 1. What we already have

The supplied files give us a substantial head start.

- `SLUS_214.42` — the PS2 MIPS R5900 ELF executable.
- `arika.zip` — existing reverse-engineering research, format parsers, validation fixtures, exporters, C++ structure declarations and Rust implementations.

The research identifies the game as Super Dragon Ball Z, and contains an inventory of 1,934 extracted game resources.

### Existing format research

| Format | Count | Intended use                  |
| ------ | ----- | ----------------------------- |
| STG    | 13    | Stage scene graphs            |
| EBZ    | 147   | Effects                       |
| PIX    | 729   | Textures                      |
| MDL    | 475   | Models                        |
| PMT    | 57    | Materials and motion packages |
| ANI    | 20    | Character animation data      |
| PAS    | 7     | Pose data                     |
| CHT    | 43    | Facial animation              |
| FON    | 1     | Game font                     |
| ASE    | 27    | Sound banks                   |
| AMS    | 54    | Music and audio sequences     |
| SPT    | 77    | Game scripts                  |
| HIT    | 14    | Collision resources           |

These formats should become the foundation of our native resource system.

### Existing capabilities we can reuse

The research already includes:

- INFO.DAT and GAME.DAT extraction and reconstruction.
- FMDL geometry, hierarchy and skinning.
- PIX/TIM2 texture decoding.
- RFSM/MOT2 animation decoding.
- STG stage parsing and scene assembly.
- HIT collision extraction.
- CHT facial animation research.
- ASE and AMS PS2 ADPCM audio decoding.
- SPT script decoding.
- EBZ effect research.
- FFON font decoding.
- glTF exporters and validation fixtures.

Importantly, the existing research has validated all 476 model payloads and all 57 RFSM packages, although runtime rendering and animation still need to be reproduced.

We should preserve this research rather than rediscovering formats.

# 2. Proposed technology stack

| Component        | Technology                        |
| ---------------- | --------------------------------- |
| Language         | C++20                             |
| Platform layer   | SDL3                              |
| Rendering        | SDL3 GPU                          |
| Shader language  | HLSL / SPIR-V                     |
| Audio            | SDL3 Audio                        |
| Input            | SDL3 Gamepad                      |
| Build system     | CMake + Ninja                     |
| Compiler         | Clang / MSVC                      |
| Image processing | Custom PIX/TIM2 decoder           |
| Model loading    | Native FMDL loader                |
| Animation        | Custom skeletal animation system  |
| Physics          | Custom fighting-game simulation   |
| UI               | Custom immediate-mode 2D renderer |
| Testing          | CTest                             |

### Why SDL3 GPU?

I would use SDL3's GPU API rather than building the renderer directly around OpenGL.

It provides access to modern graphics backends while keeping the platform layer relatively small.

This gives us a suitable foundation for:

- Skeletal animation.
- Multiple texture formats.
- Depth and stencil buffers.
- Render targets.
- Post-processing.
- Particle effects.
- Shadow rendering.
- Screen-space effects.
- Hardware instancing.

We should not attempt to reproduce the PS2's graphics hardware literally. Instead, we should reproduce its visual output using a modern rendering pipeline.

# 3. Engine architecture

I recommend separating the game into six major subsystems.

Game application

SDL3 · Window · Input · Audio · Main loop

Game runtime

States · Modes · Rules

Asset system

Archives · Models · Textures

Gameplay

Fighters · Combat · AI

Presentation

Rendering · Animation · UI

SDL3 platform backend

Graphics · Audio · Input · Timing · Filesystem

### Suggested source layout

```
super_dbz/
├── CMakeLists.txt
├── CMakePresets.json
│
├── assets/
│   ├── original/
│   ├── extracted/
│   └── overrides/
│
├── source/
│   ├── main.cpp
│   │
│   ├── core/
│   │   ├── application.cpp
│   │   ├── game_clock.cpp
│   │   ├── game_state.cpp
│   │   ├── filesystem.cpp
│   │   └── memory.cpp
│   │
│   ├── platform/
│   │   ├── sdl_window.cpp
│   │   ├── sdl_input.cpp
│   │   ├── sdl_audio.cpp
│   │   └── sdl_gpu.cpp
│   │
│   ├── renderer/
│   │   ├── renderer.cpp
│   │   ├── render_device.cpp
│   │   ├── render_target.cpp
│   │   ├── shader.cpp
│   │   ├── material.cpp
│   │   ├── texture.cpp
│   │   ├── model_renderer.cpp
│   │   ├── skeletal_renderer.cpp
│   │   ├── effect_renderer.cpp
│   │   └── post_process.cpp
│   │
│   ├── resources/
│   │   ├── archive.cpp
│   │   ├── resource_manager.cpp
│   │   ├── mdl_loader.cpp
│   │   ├── pix_loader.cpp
│   │   ├── pmt_loader.cpp
│   │   ├── ani_loader.cpp
│   │   ├── cht_loader.cpp
│   │   ├── stg_loader.cpp
│   │   ├── hit_loader.cpp
│   │   ├── ebz_loader.cpp
│   │   ├── spt_loader.cpp
│   │   ├── ase_loader.cpp
│   │   ├── ams_loader.cpp
│   │   └── fon_loader.cpp
│   │
│   ├── animation/
│   │   ├── skeleton.cpp
│   │   ├── animation_clip.cpp
│   │   ├── animation_controller.cpp
│   │   ├── animation_blender.cpp
│   │   └── facial_animation.cpp
│   │
│   ├── gameplay/
│   │   ├── game_session.cpp
│   │   ├── fighter.cpp
│   │   ├── fighter_controller.cpp
│   │   ├── combat.cpp
│   │   ├── hit_detection.cpp
│   │   ├── movement.cpp
│   │   ├── character_state.cpp
│   │   ├── ai.cpp
│   │   └── camera.cpp
│   │
│   ├── effects/
│   │   ├── effect_manager.cpp
│   │   ├── particle_system.cpp
│   │   └── effect_instance.cpp
│   │
│   ├── audio/
│   │   ├── audio_manager.cpp
│   │   ├── adpcm_decoder.cpp
│   │   └── music_sequencer.cpp
│   │
│   ├── ui/
│   │   ├── ui_renderer.cpp
│   │   ├── menu_manager.cpp
│   │   ├── character_select.cpp
│   │   ├── hud.cpp
│   │   └── font.cpp
│   │
│   └── tools/
│       ├── asset_inspector.cpp
│       └── asset_converter.cpp
│
└── tests/
    ├── test_archive.cpp
    ├── test_models.cpp
    ├── test_animation.cpp
    ├── test_stages.cpp
    └── test_combat.cpp
```

The layout is deliberately modular. The resource loaders should not depend on SDL, and gameplay should not depend on the renderer.

This also allows us to run format tests without opening a game window.

# 4. Development phases

I would approach this incrementally. Each phase should produce something executable rather than spending months building infrastructure without seeing the game.

## Phase 1 — Foundation and executable analysis

First milestone

Objective: Get the application running and establish a reliable asset pipeline.

Tasks:

1. Create the CMake project.
2. Integrate SDL3 and SDL3 GPU.
3. Create a window and rendering context.
4. Implement a fixed-timestep game loop.
5. Implement keyboard and gamepad input.
6. Implement a resource filesystem.
7. Implement INFO.DAT and GAME.DAT loading.
8. Port the existing archive decoder to C++.
9. Build an asset inspection command-line tool.
10. Establish binary parsing utilities.

The original ELF should remain our reference for:

- Function addresses and call relationships.
- Game state transitions.
- Resource loading order.
- Animation and gameplay logic.
- Timing and update functions.
- Collision and damage calculations.

Deliverable: A working SDL3 application that can open the original game archives and enumerate their contents.

## Phase 2 — Native asset loaders

Objective: Load original game assets directly, without intermediate glTF conversion.

| Priority | Loader  | Result                        |
| -------- | ------- | ----------------------------- |
| 1        | PIX     | Textures                      |
| 2        | MDL     | Models and skeletons          |
| 3        | PMT     | Materials and motion packages |
| 4        | ANI     | Character animation data      |
| 5        | STG     | Stage geometry and placement  |
| 6        | HIT     | Collision geometry            |
| 7        | CHT     | Facial animation              |
| 8        | EBZ     | Effects                       |
| 9        | FON     | Font                          |
| 10       | ASE/AMS | Sound and music               |
| 11       | SPT     | Gameplay scripts              |

We should port the existing Rust implementations where useful, using their C++ equivalents as references.

One important design decision: preserve the original binary representations wherever possible. Do not prematurely convert everything into a simplified generic format.

Deliverable: A native asset viewer capable of loading original characters, textures, animations and stages.

## Phase 3 — Rendering engine

Objective: Reproduce the original game's visual presentation.

[Super Dragon Ball Z (2005) - MobyGames](https://images.openai.com/static-rsc-4/wFoN7itvykx6a6U6xKlvC77RVM3HL7QRBSVnsZa2XRBkiTUeOudK6RBNp-uZDEUWMaEv1w9VuDmgsz7y4G9Wx47HzQ1q62pxHZiPq4Ph6pq8PV1EHsxSC-7J_kemX0VXM81WvMo5fL1VMULNV6k6KxHuZuDMJUSTkkKlqRABcQo?purpose=inline)

[Super Dragon Ball Z for PlayStation 2](https://images.openai.com/static-rsc-4/5KzTBI6vqzyuD6JSDJJMccthh6NuRlmIXcxMnnEl7kQ2FORMGsLSfVj9CqxNNyJusQAQpfWeLvpsioQto1PMjW5Wqk6QkL35n9E51SrWE2auqd5zj0wMB5B3TikdFPFup8O-Wjhj2eqoGszfwH7KtWiZRseetHw7-wN-_XWx5-I?purpose=inline)

[- YouTube](https://images.openai.com/static-rsc-4/qElbKGm5TOf21OktkOGK14n43aNQ2WVeAnBmJaVmX6aREhNkiXrNJ_ZXny-ugwYFKY5505VVmioYVBtuLMV5fzOQSnVT6CFmFxpnrFDApLWCjiDQbS3MAQdU-44HJ6mFvx_c9xjJ69P8aFD01wFyr2qpzZIAJ330zdvYiItQWcQ?purpose=inline)

7

Implementation order:

1. Texture upload and sampling.
2. Static mesh rendering.
3. Material system.
4. Skeletal mesh rendering.
5. Bone hierarchy and skinning.
6. Animation playback.
7. Stage rendering.
8. Lighting.
9. Shadows.
10. Particle and special effects.
11. Transparency and blending.
12. Post-processing.

We should initially target a PS2-like visual presentation rather than attempting to modernize the art style.

That means retaining:

- Original texture resolution.
- Original model proportions.
- Original animation timing.
- Original colour palette.
- Original camera composition.
- Original effects and particle appearance.

Deliverable: A fully rendered character standing in an original stage, with skeletal animation.

## Phase 4 — Animation and character systems

Objective: Make characters behave and animate correctly.

This is one of the most important parts of the reconstruction.

Implement:

- Skeleton hierarchy.
- Animation clip playback.
- Animation blending.
- Animation transitions.
- Motion selection.
- Character forms.
- Facial animation.
- Voice-event synchronization.
- Character-specific animation bindings.
- Animation event callbacks.

The existing RFSM/MOT2 and ANI/CHT research provides a significant starting point.

We should build a native animation controller that can reproduce the original animation selection and transition behaviour.

Deliverable: A controllable character that can play the original animations, including transitions and facial expressions.

## Phase 5 — Fighting game mechanics

Major reverse-engineering effort

Objective: Reconstruct the actual fighting game.

This is where the project moves beyond asset rendering.

Subsystems:

| System             | Required functionality                 |
| ------------------ | -------------------------------------- |
| Character movement | Walking, running, jumping, falling     |
| Combat             | Light/heavy attacks, combos            |
| Hit detection      | Hitboxes, hurtboxes, collision         |
| Damage             | Damage calculation and scaling         |
| Defence            | Blocking, recovery, knockback          |
| Special moves      | Ki attacks, projectiles, supers        |
| Character states   | State transitions and interruption     |
| Resources          | Health, energy and other combat meters |
| AI                 | Opponent decision-making               |
| Camera             | Tracking, zoom, bounds                 |
| Round manager      | Timer, victory, defeat, rematch        |

### Critical implementation detail

We should not invent a generic fighting-game system and then try to make it look like Super Dragon Ball Z.

Instead, recover the original behaviour from the ELF.

The HIT files, animation data, SPT scripts and executable should be correlated to reconstruct:

- Attack startup frames.
- Active frames.
- Recovery frames.
- Hitbox activation.
- Damage values.
- Knockback.
- Cancel windows.
- Character-specific movement.
- Special attack conditions.

Where the original logic cannot be recovered immediately, use a provisional implementation that can later be replaced.

Deliverable: Two playable characters fighting each other with functional combat.

## Phase 6 — Effects and audio

Objective: Recreate the presentation that makes the game recognizable.

### Effects

The EBZ research already identifies the effect manager and associated classes, including `CEffectMgr`, `CEBZData` and `CEBZEffectObj`.

Implement:

- EBZ resource loading.
- Effect spawning.
- Effect transforms.
- Particle lifetime.
- Particle movement.
- Effect animation.
- Character attachment.
- Stage attachment.
- Destruction and visibility.

### Audio

Implement:

- ASE sample-bank loading.
- Sony PS ADPCM decoding.
- AMS streaming.
- Music intro/loop handling.
- Voice playback.
- Sound effect triggers.
- Animation-linked audio.
- Facial animation synchronization.

The existing audio research provides both decoding and reconstruction references, including the original block ordering of AMS streams.

Deliverable: A battle with original sound effects, music, voice and special attack effects.

## Phase 7 — Menus and game modes

Objective: Reconstruct the complete game experience.

[Daizenshuu EX - General - Podcast - Episode #0034: "Super DragonBall Z" (JP PS2) Screens](https://images.openai.com/static-rsc-4/UFaQfkamjox8wtXVDSkvskSP1jJtp73yN5TCHDjt8XTBS0yohCdSvqgjCSrhA-MTAro-iGzLvczoM5g37Bdf9cr_NXBjFzDAKw9keCxKKOgKcj2zIl-7olkDO1N7mv9oCQDtDyLyEIWBcXBNY0I1vTcTnNBtTN8wzfeinT3mBFE?purpose=inline)

[Super Dragon Ball Z definitely had one of the most interesting rosters I've ever seen. | DBZeta - DBZ Forum](https://images.openai.com/static-rsc-4/_a-oVRrMz7Ujr2DsMWO3wx_c9H7MOPpVXtirtVMG53QPBU_BXAjFbXVFzYnvYN5WhTSYh0E4easIKFUfdSkj2o8NOmLDcIhld_BFhTqAq8y8yvsizslH203hi6CXPSCt0FXof8UbiMDqnhwJ3gC8bTsTEKrcB6J1JZsecNrSles?purpose=inline)

[New colors for Saiyan saga Vegeta Mod for Super Dragon Ball Z | Super DBZ Mods](https://images.openai.com/static-rsc-4/kDVhkecXDGqmJHd35dgV3z4i6NXq8RUm_Eo6Jg2nKxXP2-vhU-8VaJRHpoRz9BszqXlVIUZW5qj6ybGJVvAdG2AOP72d5a3eEXrgl9NZhGk6H-WWxwHzM5-jzR5KB_ZoSH7dzBVsqPX-Z7rOWLGwgC3pVN2sFMMu7iqx3kZCtNI?purpose=inline)

Implement:

1. Startup sequence.
2. Main menu.
3. Character selection.
4. Character customization and forms.
5. Versus screen.
6. Stage selection.
7. Battle HUD.
8. Victory screen.
9. Game over.
10. Options.
11. Save/load progression.

The menu system should be built as a separate UI subsystem, using the original decoded font and textures.

We should preserve the original layouts and animation behaviour rather than simply creating modern menus with similar functionality.

Deliverable: A complete menu-to-battle-to-results gameplay loop.

## Phase 8 — Complete game modes

Objective: Reach feature parity with the original game.

This includes:

- Arcade mode.
- Versus mode.
- Training mode, if supported by the target release.
- Character unlocks.
- Progression.
- Save data.
- Difficulty settings.
- Round configuration.
- Character-specific sequences.
- All stages and playable characters.

We should verify the actual mode list against the supplied executable and game data rather than assume every mode from other releases is present.

Deliverable: A complete playable reconstruction.

# 5. Technical decisions that matter

There are several areas where I would make deliberate architectural decisions early.

### Fixed timestep

The game should use a fixed simulation timestep, initially targeting 60 updates per second.

```
constexpr double k_simulation_rate = 60.0;
constexpr double k_simulation_step = 1.0 / k_simulation_rate;
```

Rendering should be independent of simulation updates.

This is particularly important for fighting-game frame data, animation events, collision and combo timing.

### Coordinate system

We should establish one engine coordinate convention and convert original assets at the resource boundary.

I suggest:

- Y-up world coordinates.
- Right-handed coordinate system.
- Radians internally.
- Quaternions for skeletal rotations.
- Consistent matrix multiplication conventions.

The existing stage research already uses Y-up coordinates and provides useful transform references.

### Resource management

Original assets should be loaded directly from the extracted archive, with optional overrides.

For example:

```
assets/
├── original/
│   ├── INFO.DAT
│   └── GAME.DAT
│
├── extracted/
│   ├── ply/
│   ├── stg/
│   ├── eff/
│   └── sqm/
│
└── overrides/
    ├── ply/
    ├── stg/
    └── eff/
```

This lets us modify individual assets without modifying the original game archives.

### Binary format compatibility

Every resource loader should support:

- Bounds checking.
- Endianness handling.
- Version validation.
- Invalid pointer detection.
- Truncated resource detection.
- Diagnostic output.
- Unit tests against existing fixtures.

We should also retain unknown fields instead of discarding them. Some of these fields may contain information we have not yet identified.

# 6. Reverse-engineering priorities

I would prioritize further ELF analysis alongside development.

| Priority | Research target            | Why                    |
| -------- | -------------------------- | ---------------------- |
| Critical | Game state machine         | Overall game structure |
| Critical | Character update functions | Core gameplay          |
| Critical | Animation selection        | Character behaviour    |
| Critical | Hit detection              | Combat                 |
| High     | Movement and physics       | Fighting mechanics     |
| High     | Camera controller          | Presentation           |
| High     | EBZ runtime                | Effects                |
| High     | Script execution           | Game sequences         |
| Medium   | AI                         | Arcade gameplay        |
| Medium   | Menu state transitions     | Complete frontend      |
| Medium   | Save system                | Progression            |

One important version issue: the supplied ELF is named `SLUS_214.42`, whereas much of the archive research documents the European beta executable `SLES_541.61`. We should verify function addresses, asset names and binary layouts against the exact target release before treating the existing reverse-engineering results as definitive.

# 7. Development milestones

I would organize development around these concrete milestones.

| Milestone | Expected result                     |
| --------- | ----------------------------------- |
| M1        | SDL3 application and archive reader |
| M2        | Native texture viewer               |
| M3        | Native model viewer                 |
| M4        | Animated character viewer           |
| M5        | Stage viewer                        |
| M6        | Character controller                |
| M7        | Two-character combat                |
| M8        | Original effects and audio          |
| M9        | Character selection and menus       |
| M10       | Complete gameplay loop              |
| M11       | Remaining modes and progression     |
| M12       | Full compatibility and polish       |

The most important thing is to have something visually recognizable as early as M3.

We should avoid spending too much time implementing abstract engine infrastructure before we can render the actual game assets.

# 8. First development sprint

Recommended starting point

I would start with the following implementation sequence.

Sprint 1 — Native C++ foundation

0 / 11 completed

0%

Create CMake project with C++20 and SDL3

Configure Clang/MSVC builds

Create SDL3 window and GPU device

Implement fixed-timestep game loop

Implement keyboard and gamepad input

Implement binary reader utilities

Port INFO.DAT decoder

Port GAME.DAT archive reader

Implement resource enumeration

Load and display a PIX texture

Add automated archive tests

Once this is complete, the next sprint should implement the FMDL loader and get Goku rendered with his original textures.

# 9. My recommendation

I would make three rules for the entire project:

1. Native formats first. Load the original MDL, PIX, PMT, ANI and other formats directly. glTF should remain an inspection and interchange format, not the runtime asset format.
2. Original behaviour first. Use the ELF to recover gameplay logic rather than designing a new fighting game.
3. Continuous visual progress. Every major subsystem should contribute to a working playable build.

The existing Rust project is particularly useful as a format specification and reference implementation. We should port proven functionality into C++, not rewrite the research from scratch.

The first actual target should be a native SDL3 application displaying a fully textured and animated Goku in an original Super Dragon Ball Z stage. Once that works, we have the foundation for the rest of the game.