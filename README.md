# You Have Got Pizza

**You Have Got Pizza** is an original PRG32 cartridge game for the European RISC-V Summit 2026 demonstration track. It is a small, fully playable, Burger Time-inspired arcade game reimagined in an academic Piazza. The player is a professor preparing pizza components to feed starving students. The metaphor is simple: building cool teaching tools makes students hungry for knowledge and foolish enough to try hard things with joy.

The game is deliberately implemented twice:

- `assembly/game.S`: RISC-V assembly using the PRG32 cartridge ABI directly. Draws with plain rectangles and single-tone beeps, kept deliberately simple as a first low-level lab.
- `c/game.c`: a pedagogically commented C version with the same gameplay structure. It additionally renders small painted, palette-indexed sprites/tiles and plays an 8-voice SID-like soundtrack through PRG32's real synth mixer -- see "Indexed-color art and SID-like audio (C version)" below.

The repository also includes original PNG graphics and WAV sound masters. No copyrighted sprites, music, names, layouts, or sounds from BurgerTime or any other commercial game are included.

## Repository layout

This is a **standalone third-party PRG32 game repository**. It is not expected to live inside the PRG32 source tree.

```text
you_have_got_pizza/
|-- README.md
|-- LICENSE
|-- assembly/
|   `-- game.S
|-- c/
|   |-- game.c
|   `-- assets_indexed.inc      (generated; see pack_indexed_assets.py)
|-- assets/
|   |-- icon.png
|   |-- screenshot.png
|   |-- generate_assets.py      (background/splash/UI reference art + WAV masters)
|   |-- generate_indexed_art.py (small palette-limited ROM sprite/tile art)
|   |-- pack_indexed_assets.py  (PNG -> c/assets_indexed.inc, via PRG32's own tools)
|   |-- generate_audio.py       (writes audio.json, the SID-like score)
|   |-- audio.json
|   |-- manifest.json
|   |-- original_art.md
|   |-- png/
|   |   |-- background_piazza_320x200.png
|   |   |-- splash_you_have_got_pizza_320x200.png
|   |   |-- ui_lives_score_icons.png
|   |   `-- rom/                (small indexed PNGs actually compiled into the cartridge)
|   |       |-- rom_professor_4frames_12x16.png
|   |       |-- rom_student_blue_4frames_12x16.png
|   |       |-- rom_student_magenta_4frames_12x16.png
|   |       |-- rom_ingredients_4x4_28x12.png
|   |       |-- rom_tile_arch_20x26.png
|   |       |-- rom_tile_stone_16x10.png
|   |       |-- rom_tile_ladder_14x12.png
|   |       |-- rom_tile_plate_80x24.png
|   |       |-- rom_title_emblem_56x48.png
|   |       `-- rom_life_icon_8x8.png
|   `-- wav/
|       |-- sfx_start_jingle.wav
|       |-- sfx_climb_up.wav
|       |-- sfx_climb_down.wav
|       |-- sfx_collect_dough.wav
|       |-- sfx_collect_sauce.wav
|       |-- sfx_collect_cheese.wav
|       |-- sfx_collect_basil.wav
|       |-- sfx_pizza_ready.wav
|       |-- sfx_student_collision.wav
|       `-- sfx_game_over.wav
|-- docs/
|   `-- build-and-publish.md
|-- metadata/
|   |-- colophon.json
|   |-- manifest.json
|   `-- metadata.json
|-- scripts/
|   |-- build.sh
|   `-- pack-store-bundle.sh
`-- tools/
    `-- build_cartridges.sh
```

## Game design

You move through a Piazza made of stone platforms and ladders. Collect the pizza ingredients in the order suggested by the level art: dough, sauce, cheese, and basil. Starving students wander across the platforms. If a student reaches you before the pizza is ready, you lose a life. When all ingredients are collected, a pizza is ready, the students are fed, and the board resets at a slightly more urgent pace.

The assembly version draws the professor and students as plain rectangles with single-tone beeps, keeping every pixel operation and every sound trivially traceable in RISC-V. The C version instead draws small, hand-painted, palette-indexed sprites and plays them over an 8-voice synth soundtrack -- see the next section for why the two versions now differ, and how to regenerate or restyle that art and music.

### Controls

Use **Joystick 1** only.

| Input | Action |
|---|---|
| LEFT / RIGHT | Walk on the current platform |
| UP / DOWN | Climb when aligned with a ladder |
| START | Restart after game over |

## PRG32 narrative

PRG32 is an educational RISC-V gaming runtime. It turns low-level programming from an abstract lecture into a visible loop: change a register, rebuild a cartridge, move a sprite, and see the result. That loop is powerful for youngsters learning computer architecture because the screen makes the consequences of state, branches, memory layout, and calling conventions tangible.

A PRG32 cartridge exports three functions:

```text
<prefix>_init
<prefix>_update
<prefix>_draw
```

`init` sets the initial state, `update` reads input and advances the model, and `draw` renders the state. This separation is a gentle bridge from C to assembly: students first see the same game logic in C, then follow the exact same responsibilities in RISC-V assembly.

For this game, the prefixes are:

| Version | Entry prefix | Exported symbols |
|---|---|---|
| Assembly | `you_have_got_pizza` | `you_have_got_pizza_init`, `you_have_got_pizza_update`, `you_have_got_pizza_draw` |
| C | `you_have_got_pizza_c` | `you_have_got_pizza_c_init`, `you_have_got_pizza_c_update`, `you_have_got_pizza_c_draw` |

## Teaching tutorial

For a complete first-year laboratory sequence, see
[`docs/tutorial-create-you-have-got-pizza.md`](docs/tutorial-create-you-have-got-pizza.md).
It guides students from design through the code-deploy-debug cycle and final
publication on the Cartridge Store.

## Development requirements

You need two separate repositories/directories:

1. this game repository;
2. a cloned PRG32 repository, used as the resident runtime and cartridge tool provider.

Example layout:

```sh
mkdir -p $HOME/src
cd $HOME/src
git clone https://github.com/riscv-prg32/PRG32.git
git clone https://github.com/riscv-prg32/YouHaveGotPizza.git
```

Install and export ESP-IDF as required by PRG32. The PRG32 documentation currently uses ESP-IDF with ESP32-C3 support for QEMU and ESP32-C6 support for the physical board.

On each new shell, source ESP-IDF before using `idf.py`:

```sh
. $HOME/esp-idf/export.sh
```

Then tell the portable helper script where PRG32 is:

```sh
export PRG32_REPO=$HOME/src/PRG32
cd $HOME/src/you_have_got_pizza
```

The default build matches the portable cartridge workflow used by DeviceDemo on
PRG32 `main`: it builds the C cartridge as a portable ABI-table
cartridge, attaches Store metadata, and writes a publishable `.prg32` file.

## Build a portable cartridge

Build for ESP32-C6 hardware:

```sh
export PRG32_REPO=$HOME/src/PRG32
export PRG32_ARCHITECTURE=esp32c6
scripts/build.sh
```

The output is:

```text
dist/you-have-got-pizza-esp32c6.prg32
```

Build for QEMU:

```sh
export PRG32_REPO=$HOME/src/PRG32
export PRG32_ARCHITECTURE=qemu
scripts/build.sh
```

The output is:

```text
dist/you-have-got-pizza-qemu.prg32
```

Current PRG32 `main` builds portable cartridges only. Both C and assembly
versions use its public timed-note audio API.

## Deploy a portable cartridge

Upload to a PRG32 ESP32-C6 board:

```sh
PYTHONPATH="$PRG32_REPO" python3 -m prg32 esp32c6 upload \
  dist/you-have-got-pizza-esp32c6.prg32 \
  --url http://192.168.4.1
```

Stage the QEMU cartridge into a PRG32 QEMU flash image:

```sh
PYTHONPATH="$PRG32_REPO" python3 -m prg32 qemu upload \
  dist/you-have-got-pizza-qemu.prg32 \
  --flash "$PRG32_REPO/build-qemu/qemu_flash.bin" \
  --partitions "$PRG32_REPO/partitions_prg32.csv"
```

## Publish a CartridgeStore bundle

Build both architecture variants, pack the flat Store bundle, then publish:

```sh
export PRG32_REPO=$HOME/src/PRG32

export PRG32_ARCHITECTURE=esp32c6
scripts/build.sh

export PRG32_ARCHITECTURE=qemu
scripts/build.sh

scripts/pack-store-bundle.sh

PYTHONPATH="$PRG32_REPO" python3 -m prg32 store publish-bundle \
  dist/you-have-got-pizza-store-bundle.zip \
  --store-url http://192.168.1.42:5080 \
  --token "$PRG32_STORE_TOKEN"
```

See `docs/build-and-publish.md` for the focused build, deploy, QEMU, and Store
workflow.

`tools/build_cartridges.sh` builds both C and assembly portable cartridges.
It can also build the resident firmware before staging or uploading them.

## Build the resident PRG32 firmware for QEMU

The helper can build the resident firmware automatically, but these are the explicit PRG32-side commands for clarity:

```sh
cd $PRG32_ROOT

idf.py -B build-qemu \
  -D SDKCONFIG=build-qemu/sdkconfig \
  -D SDKCONFIG_DEFAULTS=sdkconfig.defaults.qemu \
  set-target esp32c3

idf.py -B build-qemu \
  -D SDKCONFIG=build-qemu/sdkconfig \
  -D SDKCONFIG_DEFAULTS=sdkconfig.defaults.qemu \
  build
```

Run QEMU once to create the flash image:

```sh
idf.py -B build-qemu \
  -D SDKCONFIG=build-qemu/sdkconfig \
  -D SDKCONFIG_DEFAULTS=sdkconfig.defaults.qemu \
  qemu --graphics monitor
```

Stop QEMU after the first successful launch. This creates `build-qemu/qemu_flash.bin`.

## Build cartridges for QEMU from this standalone repository

From the game repository root:

```sh
export PRG32_ROOT=$HOME/src/PRG32
./tools/build_cartridges.sh qemu
```

The generated cartridges are written to:

```text
dist/qemu/you-have-got-pizza-asm.prg32
dist/qemu/you-have-got-pizza-c.prg32
```

To build and stage the assembly cartridge into the QEMU flash image:

```sh
./tools/build_cartridges.sh qemu --upload-qemu
```

Then run QEMU from the PRG32 repository:

```sh
cd $PRG32_ROOT
idf.py -B build-qemu \
  -D SDKCONFIG=build-qemu/sdkconfig \
  -D SDKCONFIG_DEFAULTS=sdkconfig.defaults.qemu \
  qemu --graphics monitor
```

To stage the C cartridge instead, build normally and call the PRG32 builder directly:

```sh
PYTHONPATH="$PRG32_ROOT" python3 -m prg32 qemu upload \
  dist/qemu/you-have-got-pizza-c.prg32 \
  --flash $PRG32_ROOT/build-qemu/qemu_flash.bin
```

## Build the resident PRG32 firmware for physical ESP32-C6 hardware

Again, the helper can do this automatically, but the explicit commands are:

```sh
cd $PRG32_ROOT

idf.py -B build-esp32c6 \
  -D SDKCONFIG=build-esp32c6/sdkconfig \
  -D SDKCONFIG_DEFAULTS=sdkconfig.defaults \
  set-target esp32c6

idf.py -B build-esp32c6 \
  -D SDKCONFIG=build-esp32c6/sdkconfig \
  -D SDKCONFIG_DEFAULTS=sdkconfig.defaults \
  build

idf.py -B build-esp32c6 \
  -D SDKCONFIG=build-esp32c6/sdkconfig \
  -D SDKCONFIG_DEFAULTS=sdkconfig.defaults \
  flash monitor
```

After this step the resident PRG32 firmware is on the board. Cartridge upload is normally performed over the PRG32 Wi-Fi access point.

## Build cartridges for ESP32-C6 hardware from this standalone repository

From the game repository root:

```sh
export PRG32_ROOT=$HOME/src/PRG32
./tools/build_cartridges.sh esp32c6
```

The generated cartridges are written to:

```text
dist/esp32c6/you-have-got-pizza-asm.prg32
dist/esp32c6/you-have-got-pizza-c.prg32
```

To build and upload the assembly cartridge to a running ESP32-C6 PRG32 board:

```sh
./tools/build_cartridges.sh esp32c6 --upload-hardware --url http://192.168.4.1
```

To upload the C cartridge instead:

```sh
PYTHONPATH="$PRG32_ROOT" python3 -m prg32 esp32c6 upload \
  dist/esp32c6/you-have-got-pizza-c.prg32 \
  --url http://192.168.4.1
```

## Manual portable cartridge build commands

From the PRG32 checkout, build either source with the current CLI. Use
`--architecture qemu` for the emulator and `--architecture esp32c6` for the
physical board.

```sh
cd "$PRG32_ROOT"
python3 -m prg32 cartridge build /path/to/YouHaveGotPizza/assembly/game.S \
  --portable --architecture qemu --entry-prefix you_have_got_pizza \
  --name pizza-asm --out /path/to/YouHaveGotPizza/dist/qemu/pizza-asm.prg32
```

The C source uses `c/game.c` and entry prefix `you_have_got_pizza_c`. It
`#include`s the generated `c/assets_indexed.inc`, so run
`assets/generate_indexed_art.py` and `assets/pack_indexed_assets.py` first
(see the next section). To include the SID-like soundtrack, also pack
`assets/audio.json` with `tools/prg32audio_pack.py` and pass the result via
`--audio-block`.

## Indexed-color art and SID-like audio (C version)

PRG32 caps a cartridge at **64 KiB total** -- code, sprite data, and audio
block together (`PRG32_CART_MAX_KIB` / `PRG32_CART_RAM_KIB` in PRG32's
`prg32.h`). A single full-screen 320x200 painted background at 4 or 8 bits
per pixel would be 16-64 KB by itself -- more than the entire budget. Real
'90s hardware never stored full-screen raster backgrounds for exactly this
reason: it built scenes from small, cheaply repeated tiles and sprites. The C
version follows the same approach, using PRG32's real indexed-sprite pipeline
(`prg32_sprite_draw_indexed` / `prg32_sprite_draw_bitplanes`,
`prg32_indexed_sprite_t`) instead of the flat rectangles used everywhere
else. This is why the C and assembly versions now render differently: porting
this to hand-written RV32 assembly would roughly double the size of a file
meant to stay small and traceable, so `assembly/game.S` intentionally stays
on the original rectangle/single-tone approach.

A full 320x200 build of the whole cartridge (game logic + all sprite/tile art
+ the audio block) currently comes to about 15 KB -- comfortably inside the
64 KB budget, with room for further additions.

### Regenerate everything

```sh
python3 assets/generate_assets.py          # background/splash/UI reference PNGs + WAV masters
python3 assets/generate_indexed_art.py     # small palette-limited ROM sprite/tile PNGs (assets/png/rom/)
PRG32_REPO=/path/to/PRG32 python3 assets/pack_indexed_assets.py   # -> c/assets_indexed.inc
python3 assets/generate_audio.py           # -> assets/audio.json
```

`scripts/build.sh` and `tools/build_cartridges.sh` already run the indexed-art
and audio steps automatically before building the C cartridge, so this is
only needed when iterating on art/audio without a full rebuild.

### Two tiers of art

- `assets/generate_assets.py` draws the big, full-resolution **reference**
  art (`background_piazza_320x200.png`, the splash screen, the UI icon
  sheet) and the WAV sound masters. None of this is compiled into the
  cartridge -- it exists for documentation, the Store icon, and the
  screenshot.
- `assets/generate_indexed_art.py` draws the small, palette-limited **ROM**
  art under `assets/png/rom/` that is actually compiled in: the professor and
  student walk cycles (12x16, 4 frames), the ingredient sheet (28x12, 4
  kinds x 4 frames), and small repeating environment tiles (skyline arch,
  stone platform brick, ladder rung, bottom plate) plus a painted title
  emblem and a HUD life icon. Every image is flattened onto a solid magenta
  `(255, 0, 255)` key color, painted first so it lands at palette index 0 for
  transparency.
- `assets/pack_indexed_assets.py` converts those PNGs into `c/assets_indexed.inc`
  using PRG32's own `tools/prg32_image_convert.py` packing functions -- the
  same code path PRG32's `devicedemo` cartridge uses for its indexed/bitplane
  examples. PRG32 cartridges compile exactly one C source file, so every
  sprite asset has to live in this one generated, `#include`d file; requires
  `PRG32_REPO` to point at a PRG32 checkout.

To personalize a sprite or tile: edit the matching drawing function in
`generate_indexed_art.py` (e.g. `professor_frame()`, `ingredient_frame()`,
`stone_tile()`, `title_emblem()`), keep the frame's pixel dimensions and
frame-grid layout unchanged (or update the matching entry in
`pack_indexed_assets.py`'s `ASSETS` list and the draw call in `c/game.c` if
you resize it), then rerun `generate_indexed_art.py` + `pack_indexed_assets.py`.
Each image currently stays within a 16-color budget except the title emblem,
which deliberately uses more colors to exercise PRG32's 256-color path.

The animation frame in both cartridge versions is derived from the frame counter:

```c
uint8_t anim = (frame_no >> 3) & 3u;
```

That expression is an excellent low-level programming lesson: shifting divides by a power of two, masking computes a modulo for a power-of-two frame count, and the resulting small integer selects a pose.

### SID-like audio

The C version plays PRG32's real synth mixer: four waveforms (triangle, saw,
pulse, noise) with a cutoff/resonance filter baked into the sample id,
per-instrument ADSR envelopes, up to 8 voices, and stereo panning.
`assets/generate_audio.py` writes `assets/audio.json` (instruments + one
looping tracker track); PRG32's `tools/prg32audio_pack.py` packs that into a
binary `AUDIO` block, which `scripts/build.sh` passes to the cartridge
builder via `--audio-block`. The block auto-loads when the cartridge
installs, so `c/game.c` only has to call `prg32_audio_play_track()` and
`prg32_audio_note()`.

Channel layout (channel index == instrument index for tracker-driven notes,
enforced by PRG32's tracker):

| Channels | Role |
|---|---|
| 0-4 | looping 5-voice background theme (bass, two chord pads, an arpeggio lead, a noise hi-hat) |
| 5-7 | one-shot sound effects (collect, climb, collision, pizza-ready, game-over), round-robined by `sfx()` in `c/game.c` so gameplay stingers never steal a voice from the music |

To personalize the music, edit the chord progression, bass/arpeggio patterns,
or instrument ADSR/waveform values in `generate_audio.py`, then regenerate
`audio.json` and rebuild. To personalize an effect, change the `sfx(...)`
call site in `c/game.c` (instrument, MIDI note, volume, duration, pan) --
see `move_player()`, `collect_ingredients()`, and `check_collisions()`.

Note: mono-vs-stereo output is a resident-firmware build setting
(`prg32_audio_set_mode` is not exposed to portable cartridges), so a cartridge
cannot force stereo at runtime -- the panning here shapes the mix either way,
but a true stereo image needs the firmware itself built with stereo output
enabled.

The original WAV masters under `assets/wav/` and the `write_wav()` calls in
`generate_assets.py` are unchanged and still documented as before, in case
you want a sample-based (rather than synth-based) starting point.

### Copyright hygiene for personalized repositories

When publishing a personalized fork on GitHub:

- use original drawings, generated shapes, or assets you have permission to redistribute;
- do not copy sprites, sound effects, logos, level art, or music from commercial games;
- write a short provenance note in `assets/original_art.md`;
- regenerate `assets/manifest.json` after changes;
- keep licenses clear for every contributed asset.

## Helper script reference

```sh
./tools/build_cartridges.sh [qemu|esp32c6] [options]
```

Options:

| Option | Meaning |
|---|---|
| `--prg32-root DIR` | Path to a cloned PRG32 repository. Equivalent to setting `PRG32_ROOT`. |
| `--out-dir DIR` | Where to write `.prg32` cartridges. Default: `dist/<target>`. |
| `--skip-firmware` | Do not rebuild the resident PRG32 firmware. Useful when the resident firmware is already built. |
| `--upload-qemu` | Stage the assembly cartridge into `qemu_flash.bin`. |
| `--upload-hardware` | Upload the assembly cartridge to ESP32-C6 over Wi-Fi. |
| `--url URL` | Hardware upload URL. Default: `http://192.168.4.1`. |

Examples:

```sh
./tools/build_cartridges.sh qemu --prg32-root ../PRG32
./tools/build_cartridges.sh qemu --skip-firmware --upload-qemu
./tools/build_cartridges.sh esp32c6 --upload-hardware --url http://192.168.4.1
```

## Embedding temporarily in PRG32 firmware for a lab

Cartridges are the preferred demo path, but a lab may also embed the game in firmware. Copy or reference exactly one source file from this repository in PRG32's `main/CMakeLists.txt`:

```cmake
idf_component_register(
    SRCS
        "main.c"
        "/absolute/path/to/you_have_got_pizza/c/game.c"
    REQUIRES prg32
    INCLUDE_DIRS "."
)
```

The assembly version is better demonstrated as a cartridge because that keeps the ABI boundary explicit.

## Teaching notes

This project is intentionally compact. It is not a generic game engine. That is a feature for a summit demo and for a classroom: a student can read the C version, then open the assembly version and recognize the same ideas: tables, counters, loops, conditionals, calls, and state variables.

Suggested lab path:

1. play the C cartridge;
2. change a speed, a color, or a sound;
3. inspect the same idea in assembly;
4. change one rectangle in the animated sprite;
5. rebuild and deploy to QEMU;
6. deploy the final cartridge to the ESP32-C6 board.

The reward is immediate: a low-level register-level change feeds hungry students with pizza.
