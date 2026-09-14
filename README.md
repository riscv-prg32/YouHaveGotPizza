# You Have Got Pizza

**You Have Got Pizza** is an original PRG32 cartridge game for the European RISC-V Summit 2026 demonstration track. It is a small, fully playable, Burger Time-inspired arcade game reimagined in an academic Piazza. The player is a professor preparing pizza components to feed starving students. The metaphor is simple: building cool teaching tools makes students hungry for knowledge and foolish enough to try hard things with joy.

The game is deliberately implemented twice:

- `assembly/game.S`: RISC-V assembly using the PRG32 cartridge ABI directly.
- `c/game.c`: a pedagogically commented C version with the same gameplay structure.

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
|   `-- game.c
|-- assets/
|   |-- icon.png
|   |-- screenshot.png
|   |-- generate_assets.py
|   |-- manifest.json
|   |-- original_art.md
|   |-- png/
|   |   |-- background_piazza_320x200.png
|   |   |-- splash_you_have_got_pizza_320x200.png
|   |   |-- sprite_professor_4frames_12x16.png
|   |   |-- sprite_student_blue_4frames_12x16.png
|   |   |-- sprite_student_magenta_4frames_12x16.png
|   |   |-- ingredients_4x4_28x12.png
|   |   |-- tiles_platform_ladder_plate.png
|   |   `-- ui_lives_score_icons.png
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

The professor and students use four-frame animated walking sprites. In the current educational runtime code they are drawn as rectangles, because this keeps every pixel operation visible in C and in RISC-V assembly. The PNG sprite sheets under `assets/png` are the master artwork and customization reference for future bitmap-backed variants.

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

The C source uses `c/game.c` and entry prefix `you_have_got_pizza_c`.

## Personalizing graphics and sounds

The asset directory is designed so students can modify the game without touching gameplay first.

### Regenerate the original assets

The current PNG and WAV files are generated. Rebuild them with:

```sh
cd assets
python3 generate_assets.py
```

This rewrites `assets/png`, `assets/wav`, and `assets/manifest.json`.

### Change the splash screen

Edit the `draw_splash()` function in `assets/generate_assets.py`, then regenerate:

```sh
cd assets
python3 generate_assets.py
```

Keep the splash image at **320x200** pixels to match the PRG32 graphics viewport:

```text
assets/png/splash_you_have_got_pizza_320x200.png
```

Suggested student exercises:

- change the title typography;
- draw a different pizza symbol;
- add the name of a class, lab, or RISC-V event;
- keep the artwork original and avoid logos or characters from other games.

### Change the Piazza background

Edit `draw_background()` in `assets/generate_assets.py`. The output file is:

```text
assets/png/background_piazza_320x200.png
```

The runtime C and assembly versions currently redraw the background procedurally for clarity. To keep code and art aligned, update these routines after changing the background:

- `draw_piazza()` in `c/game.c`;
- `draw_piazza_asm` in `assembly/game.S`.

A good teaching workflow is to first change the PNG, discuss the composition, and then translate the same visual ideas into rectangle calls in C or assembly.

### Change animated sprites

The animated sprite sheets use a fixed frame layout:

| File | Format |
|---|---|
| `sprite_professor_4frames_12x16.png` | 4 horizontal frames, each 12x16 pixels |
| `sprite_student_blue_4frames_12x16.png` | 4 horizontal frames, each 12x16 pixels |
| `sprite_student_magenta_4frames_12x16.png` | 4 horizontal frames, each 12x16 pixels |
| `ingredients_4x4_28x12.png` | 4 rows of ingredients, 4 animation frames per row |

To personalize the professor or students, edit these functions in `assets/generate_assets.py`:

- `sprite_player_frame(frame)`;
- `sprite_student_frame(frame, shirt)`;
- `ingredient_icon(kind, frame)`.

Then regenerate the assets. If you want the running cartridge to match the new sprites exactly, also update:

- `draw_player()` and `draw_enemy()` in `c/game.c`;
- `draw_player_asm` and `draw_enemies_asm` in `assembly/game.S`.

The animation frame in both cartridge versions is derived from the frame counter:

```c
uint8_t anim = (frame_no >> 3) & 3u;
```

That expression is an excellent low-level programming lesson: shifting divides by a power of two, masking computes a modulo for a power-of-two frame count, and the resulting small integer selects a pose.

### Change sound effects

The WAV files are generated by `write_wav()` calls in `assets/generate_assets.py`. Each sound is a list of `(frequency_hz, duration_seconds)` notes. For example:

```python
write_wav("sfx_pizza_ready.wav", [(784, .08), (988, .08), (1175, .08), (1568, .20)])
```

To personalize sounds:

1. edit the note lists;
2. regenerate with `python3 generate_assets.py`;
3. keep sounds short, mono, and simple for embedded friendliness.

The cartridge converts short tone frequencies to MIDI notes with `play_tone()`
in C and `pizza_audio_note` in assembly. Update their frequency arguments when
the WAV masters change:

- climb sounds: `move_player()` in C and `move_player_asm` in assembly;
- collect sounds: `collect_ingredients()` in C and `collect_asm` in assembly;
- collision sounds: `check_collisions()` in C and `collide_asm` in assembly;
- pizza-ready sound: completion branch in both versions.

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
