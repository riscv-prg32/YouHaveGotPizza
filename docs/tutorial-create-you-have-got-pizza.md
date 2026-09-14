# Teaching Tutorial: Create, Test, and Publish You Have Got Pizza

This tutorial guides first-year computer science and computer engineering
students through the complete creation of **You Have Got Pizza**, a small PRG32
cartridge game. The exercise is an imitation game: students begin from an
existing working idea, reconstruct it step by step, and then personalize it with
their own design decisions while keeping the technical architecture sound.

The tutorial is organized as one-hour laboratories. Each laboratory has a
practical deliverable, a short conceptual target, and a visible test. The goal
is not only to finish a game, but to learn a professional habit: design a small
change, implement it, deploy it, observe the result, and debug from evidence.

## Learning Outcomes

By the end of the sequence, students should be able to:

- explain the PRG32 cartridge lifecycle: `init`, `update`, and `draw`;
- organize simple game state in C using constants, arrays, and structures;
- read joystick input and convert it into movement on a discrete level map;
- implement collision, scoring, lives, reset, and game-over logic;
- draw a complete 320x200 scene with primitive graphics operations;
- relate C control flow to the equivalent RISC-V assembly implementation;
- build portable cartridges for ESP32-C6 hardware and QEMU;
- package metadata, icon, screenshot, and architecture variants for the
  Cartridge Store;
- publish a cartridge responsibly, including license and asset provenance.

## Course Setting

Recommended preparation:

- one introductory lecture on C variables, functions, arrays, and `struct`;
- one introductory lecture on CPU state, memory, and the fetch-execute cycle;
- a working PRG32 checkout and, when available, one PRG32 ESP32-C6 board per
  small group;
- QEMU available for students who do not have immediate hardware access.

Students may work in pairs. One student acts as the driver, typing and running
the build; the other acts as the observer, reading compiler messages, checking
the expected behavior, and keeping the lab notebook. Swap roles at least once
per laboratory.

## The Code-Deploy-Debug Cycle

Every laboratory uses the same disciplined loop. This is the most important
professional habit in the exercise.

1. **State the expected behavior.**
   Write one sentence in the lab notebook before touching the code. Example:
   "When the player overlaps a dough ingredient on the same row, the score
   increases by 25 and the ingredient changes to collected."

2. **Edit one small part.**
   Change a single function or a small related group of constants. Avoid
   changing movement, rendering, and packaging in the same step.

3. **Build locally.**
   For ESP32-C6:

   ```bash
   export PRG32_REPO=$HOME/src/PRG32
   export PRG32_ARCHITECTURE=esp32c6
   scripts/build.sh
   ```

   For QEMU:

   ```bash
   export PRG32_REPO=$HOME/src/PRG32
   export PRG32_ARCHITECTURE=qemu
   scripts/build.sh
   ```

4. **Deploy to the available target.**
   Upload to hardware:

   ```bash
   PYTHONPATH="$PRG32_REPO" python3 -m prg32 esp32c6 upload \
     dist/you-have-got-pizza-esp32c6.prg32 \
     --url http://192.168.4.1
   ```

   Stage for QEMU:

   ```bash
   PYTHONPATH="$PRG32_REPO" python3 -m prg32 qemu upload \
     dist/you-have-got-pizza-qemu.prg32 \
     --flash "$PRG32_REPO/build-qemu/qemu_flash.bin" \
     --partitions "$PRG32_REPO/partitions_prg32.csv"
   ```

5. **Observe and record.**
   Test only the behavior named in step 1. Record pass/fail and one visible
   fact from the screen, serial monitor, or QEMU window.

6. **Debug from the symptom.**
   If the behavior is wrong, classify the fault before editing:

   | Symptom | Likely location |
   |---|---|
   | Build fails | syntax, missing include, wrong symbol name |
   | Cartridge uploads but does not start | entry prefix or metadata mismatch |
   | Player moves incorrectly | input mask, coordinate update, bounds |
   | Object appears in wrong place | row table, draw coordinates, sprite size |
   | Collision feels unfair | hit-box width, row equality, reset timing |
   | Store rejects the bundle | manifest fields, missing architecture files, token |

The loop is deliberately short. In a first-year course, many bugs become
unmanageable only because too many changes are made before the next test.

## Laboratory 1: Understand the Game and the Cartridge Contract

Estimated duration: 1 hour.

### Goal

Students understand what the game is, what makes it original, and how PRG32
calls a cartridge.

### Activities

1. Play or watch the finished cartridge.
2. Read the first comment block in `c/game.c`.
3. Identify the three exported cartridge functions:
   `you_have_got_pizza_c_init`, `you_have_got_pizza_c_update`, and
   `you_have_got_pizza_c_draw`.
4. Draw a simple loop diagram:
   `init` once, then repeated `update` and `draw`.
5. Discuss why `update` and `draw` are separated.

### Deliverable

A one-page design note containing:

- the player's objective;
- the losing condition;
- the three cartridge functions and their responsibilities;
- one proposed personal variation that remains copyright-clean.

### Checkpoint

The student can answer: "Which function changes the score, and which function
draws it?"

### Files and Code to Add

File: `c/game.c`.

Add this small comment block above the three exported functions. It does not
change the cartridge behavior; it gives the student a permanent map of the
runtime contract inside the source file.

```c
/*
 * PRG32 cartridge contract.
 *
 * The resident PRG32 firmware does not know our whole program. It only needs
 * three agreed entry points. This is similar to a laboratory instrument with
 * three buttons: reset the experiment, advance the experiment, and display the
 * current result.
 */
```

Then ask students to locate, not rewrite, these functions:

```c
void you_have_got_pizza_c_init(void) {
    /* Called once when the cartridge starts or restarts. */
    start_new_game();
}

void you_have_got_pizza_c_update(void) {
    /*
     * Called repeatedly. This function is responsible for time and rules:
     * input, movement, collection, enemies, collisions, and game over.
     */
}

void you_have_got_pizza_c_draw(void) {
    /*
     * Called repeatedly after update. This function converts memory state into
     * pixels, but should not change the rules of the game.
     */
}
```

## Laboratory 2: Prepare the Workspace and Build the First Cartridge

Estimated duration: 1 hour.

### Goal

Students build the unmodified cartridge and learn the local directory layout.

### Activities

1. Clone or locate both repositories:

   ```bash
   mkdir -p $HOME/src
   cd $HOME/src
   git clone https://github.com/riscv-prg32/PRG32.git
   git clone https://github.com/riscv-prg32/YouHaveGotPizza.git
   ```

2. Export ESP-IDF as required by PRG32:

   ```bash
   . $HOME/esp-idf/export.sh
   ```

3. Enter the game repository and configure the PRG32 path:

   ```bash
   cd $HOME/src/YouHaveGotPizza
   export PRG32_REPO=$HOME/src/PRG32
   ```

4. Build the portable cartridge:

   ```bash
   export PRG32_ARCHITECTURE=esp32c6
   scripts/build.sh
   ```

5. Repeat for QEMU:

   ```bash
   export PRG32_ARCHITECTURE=qemu
   scripts/build.sh
   ```

### Deliverable

Two generated files:

- `dist/you-have-got-pizza-esp32c6.prg32`;
- `dist/you-have-got-pizza-qemu.prg32`.

### Debug Notes

If the script cannot find `prg32/__main__.py`, set `PRG32_REPO` to the
current PRG32 `main` checkout and try again.

### Files and Code to Add

File: `scripts/build.sh`.

Students should not replace the repository script during the normal lab, but
they should understand the essential commands it performs. The following is a
minimal, pedagogically commented version suitable for a notebook or a temporary
classroom script named `scripts/build-lab.sh`.

```bash
#!/usr/bin/env bash
set -euo pipefail

# The game repository is the parent directory of this script.
# Keeping the path computed here lets the script run from any shell directory.
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# PRG32_REPO points to the resident runtime and cartridge tool provider.
# If the variable is not set, assume PRG32 is next to this repository.
prg32_repo="${PRG32_REPO:-"$repo_dir/../PRG32"}"

# In this course we build the C implementation first because it is easier to
# read, modify, and debug before comparing with assembly.
source_file="$repo_dir/c/game.c"
entry_prefix="you_have_got_pizza_c"

# The architecture name is part of the output filename so the Store can carry
# separate cartridges for hardware and QEMU.
architecture="${PRG32_ARCHITECTURE:-esp32c6}"
out_file="$repo_dir/dist/you-have-got-pizza-$architecture.prg32"

mkdir -p "$repo_dir/dist"

(cd "$prg32_repo" && python3 -m prg32 cartridge build \
  "$source_file" \
  --entry-prefix "$entry_prefix" \
  --name "you-have-got-pizza" \
  --portable \
  --architecture "$architecture" \
  --out "$out_file")
```

File: shell environment, not committed source.

Students add these commands to each new terminal session:

```bash
# Make PRG32 tools visible to the game build script.
export PRG32_REPO=$HOME/src/PRG32

# Choose exactly one target before each build.
export PRG32_ARCHITECTURE=esp32c6
# export PRG32_ARCHITECTURE=qemu
```

## Laboratory 3: Model the Game State

Estimated duration: 1 hour.

### Goal

Students recognize the data model behind a visible game.

### Activities

1. Inspect the constants at the top of `c/game.c`: screen size, actor size,
   number of rows, ladders, ingredients, enemies, and lives.
2. Study the `Actor` and `Ingredient` structures.
3. Locate these global variables: `player`, `enemies`, `ingredients`,
   `frame_no`, `score`, `lives`, `fed_students`, `message_timer`, and
   `game_over`.
4. Fill a table in the lab notebook:

   | Variable | Type | Meaning | Changes in |
   |---|---|---|---|
   | `score` | `uint16_t` | player score | collection, pizza completion |
   | `lives` | `uint8_t` | remaining attempts | collision, new game |
   | `frame_no` | `uint32_t` | animation clock | every update |

5. Change one harmless constant, such as `MAX_LIVES`, rebuild, and observe the
   HUD.

### Deliverable

A completed state table and one verified constant change.

### Concept Link

This laboratory connects directly to computer architecture: a game is state in
memory plus instructions that transform it. The screen is a visible projection
of that memory.

### Files and Code to Add

File: `c/game.c`.

Add or study the following declarations near the top of the file. The comments
turn each variable into a named piece of the machine state.

```c
#define SCREEN_W 320      /* PRG32 visible width in pixels. */
#define SCREEN_H 200      /* PRG32 visible height in pixels. */
#define NUM_ROWS 4        /* Four horizontal platforms in the Piazza. */
#define NUM_LADDERS 4     /* Four vertical passages between platforms. */
#define NUM_INGREDIENTS 8 /* Eight collectible pizza components. */
#define NUM_ENEMIES 4     /* Four hungry students create pressure. */
#define MAX_LIVES 3       /* The player gets three attempts per game. */

typedef struct {
    int16_t x;      /* Horizontal pixel coordinate of the actor. */
    int16_t y;      /* Vertical pixel coordinate derived from the row. */
    int8_t row;     /* Logical platform row; easier than free 2D physics. */
    int8_t alive;   /* Kept explicit so the model can grow later. */
} Actor;

typedef struct {
    int16_t x;        /* Ingredient center on its platform. */
    int8_t row;       /* Platform where the ingredient can be collected. */
    uint8_t kind;     /* 0 dough, 1 sauce, 2 cheese, 3 basil. */
    uint8_t collected;/* Zero means visible and collectible. */
} Ingredient;
```

Then change one constant for a controlled experiment:

```c
#define MAX_LIVES 5
```

The expected result is only a HUD change at the beginning of a new game. If
movement, collision, or drawing changes, the student changed too much.

## Laboratory 4: Initialize the World

Estimated duration: 1 hour.

### Goal

Students implement reproducible starting conditions.

### Activities

1. Read `setup_ingredients`, `reset_player`, `reset_enemies`, and
   `start_new_game`.
2. Draw the four platform rows and four ladder x-positions on graph paper.
3. Modify one ingredient position in `setup_ingredients`.
4. Rebuild, deploy, and verify that only that ingredient moved.
5. Restore the original position or choose a new position that improves the
   level.

### Deliverable

A level-map sketch showing rows, ladders, player start, enemies, and
ingredients.

### Debug Notes

Coordinate bugs are common. PRG32 uses a top-left origin: increasing `x` moves
right, increasing `y` moves down. The game stores platform heights in `row_y`,
then derives actor `y` positions from the current row.

### Files and Code to Add

File: `c/game.c`.

Add or modify the initialization helpers as shown below. The comments explain
why the game uses tables: a table makes the level visible in one compact place.

```c
static const int16_t row_y[NUM_ROWS] = { 48, 88, 128, 168 };
static const int16_t ladder_x[NUM_LADDERS] = { 32, 104, 188, 272 };

static void setup_ingredients(void) {
    /*
     * Each ingredient is placed by a pair of tables:
     * x[i] gives its horizontal position, r[i] gives its platform row.
     * This is the simplest level editor a first-year C program can have.
     */
    static const int16_t x[NUM_INGREDIENTS] = {
        60, 132, 220, 276, 84, 164, 244, 116
    };
    static const int8_t r[NUM_INGREDIENTS] = {
        0, 0, 0, 1, 2, 2, 3, 3
    };

    for (uint8_t i = 0; i < NUM_INGREDIENTS; ++i) {
        ingredients[i].x = x[i];
        ingredients[i].row = r[i];
        ingredients[i].kind = i & 3u; /* Cycle through dough, sauce, cheese, basil. */
        ingredients[i].collected = 0; /* A new level begins with all items visible. */
    }
}
```

For the one-line experiment, change the first x-coordinate:

```c
static const int16_t x[NUM_INGREDIENTS] = {
    72, 132, 220, 276, 84, 164, 244, 116
};
```

Only the first ingredient should move. That is the evidence that the data table
is connected to rendering without disturbing the rest of the program.

## Laboratory 5: Read Input and Move the Player

Estimated duration: 1 hour.

### Goal

Students convert joystick bits into controlled movement.

### Activities

1. Read `you_have_got_pizza_c_update`.
2. Locate `prg32_input_read` and `BTN_MOVE_MASK`.
3. Study `move_player`.
4. Identify these movement rules:
   - left and right change `player.x`;
   - bounds keep the actor inside the visible screen;
   - up and down work only when the player is aligned with a ladder;
   - row changes update `player.y`.
5. Experiment with the horizontal speed by changing `3` to `2` or `4`.

### Deliverable

A short test report:

- expected speed change;
- observed speed change;
- whether ladder alignment still feels fair.

### Debug Notes

If the player jumps rows without pressing up or down, inspect the branch that
checks `PRG32_BTN_UP` and `PRG32_BTN_DOWN`. If the player cannot climb, inspect
the distance threshold in `nearest_ladder_index`.

### Files and Code to Add

File: `c/game.c`.

Add or study the input mask and movement helper. Notice how the function never
draws; it only updates the model.

```c
#define BTN_MOVE_MASK \
    (PRG32_BTN_LEFT | PRG32_BTN_RIGHT | PRG32_BTN_UP | PRG32_BTN_DOWN)

static void move_player(uint32_t input) {
    /*
     * Horizontal movement is continuous in pixels. Vertical movement is
     * discrete in rows, because the platform game is intentionally simple.
     */
    if (input & PRG32_BTN_LEFT) {
        player.x -= 3;
    }
    if (input & PRG32_BTN_RIGHT) {
        player.x += 3;
    }

    /* Clamp the player inside the screen so later drawing stays valid. */
    if (player.x < 8) player.x = 8;
    if (player.x > SCREEN_W - PLAYER_W - 8) {
        player.x = SCREEN_W - PLAYER_W - 8;
    }

    int ladder = nearest_ladder_index(player.x + PLAYER_W / 2);
    if (ladder >= 0) {
        /*
         * We snap to the ladder only when the student actively presses up or
         * down. This prevents accidental row changes while walking.
         */
        if ((input & PRG32_BTN_UP) && player.row > 0) {
            player.x = ladder_x[ladder] - PLAYER_W / 2;
            player.row--;
            play_tone(330, 18);
        } else if ((input & PRG32_BTN_DOWN) && player.row < NUM_ROWS - 1) {
            player.x = ladder_x[ladder] - PLAYER_W / 2;
            player.row++;
            play_tone(220, 18);
        }
    }

    /* Convert the logical row back into a pixel coordinate for drawing. */
    player.y = row_y[player.row] - PLAYER_H;
}
```

For the speed experiment, change both `3` values to `2` for a slower professor
or `4` for a faster professor. Change them together so left and right remain
symmetric.

## Laboratory 6: Draw the Piazza and the Actors

Estimated duration: 1 hour.

### Goal

Students learn that rendering is a sequence of deliberate drawing operations.

### Activities

1. Read `draw_piazza`, `draw_ingredient`, `draw_player`, `draw_enemy`,
   `draw_hud`, and `draw_recipe_hint`.
2. Identify the draw order. Background comes first; player and enemies come
   after platforms and ingredients; HUD comes near the end.
3. Change one color constant and rebuild.
4. Add one small rectangle to the Piazza background.
5. Verify that the new drawing operation does not hide the player, enemies, or
   ingredients.

### Deliverable

A screenshot or photo of the modified scene, plus the exact function where the
change was made.

### Concept Link

Draw order is the simplest form of layering. Later graphics systems provide
sprites, tiles, depth buffers, and compositors, but the basic question is the
same: what is drawn first, and what is drawn over it?

### Files and Code to Add

File: `c/game.c`.

Add or modify the Piazza drawing function. The comments name the visual layer
created by each group of rectangles.

```c
static void draw_piazza(void) {
    prg32_gfx_clear(COLOR_SKY); /* Layer 1: clear the previous frame. */

    /* Layer 2: a simple skyline to tell the player where the game happens. */
    prg32_gfx_rect(0, 24, 320, 26, 0x632c);
    for (int x = 16; x < 320; x += 40) {
        prg32_gfx_rect(x, 8, 18, 42, 0x8430);
        prg32_gfx_rect(x + 5, 18, 8, 16, PRG32_COLOR_BLACK);
    }

    /* Layer 3: platforms. Actors stand just above these rows. */
    for (uint8_t r = 0; r < NUM_ROWS; ++r) {
        int y = row_y[r];
        prg32_gfx_rect(8, y, 304, 5, COLOR_STONE_DK);
        prg32_gfx_rect(8, y + 5, 304, 5, COLOR_STONE);
    }

    /* Layer 4: ladders connect the logical rows. */
    for (uint8_t i = 0; i < NUM_LADDERS; ++i) {
        int x = ladder_x[i];
        prg32_gfx_rect(x - 6, row_y[0], 3, row_y[3] - row_y[0] + 8, COLOR_STONE_DK);
        prg32_gfx_rect(x + 5, row_y[0], 3, row_y[3] - row_y[0] + 8, COLOR_STONE_DK);
        for (int y = row_y[0] + 6; y < row_y[3] + 8; y += 12) {
            prg32_gfx_rect(x - 6, y, 14, 3, COLOR_STONE);
        }
    }
}
```

For the visual experiment, add one original detail near the end of
`draw_piazza`, before actors are drawn:

```c
/* A small classroom banner: decorative, but safely behind all moving actors. */
prg32_gfx_rect(132, 30, 58, 7, COLOR_CHEESE);
prg32_gfx_text8(136, 29, "LAB", PRG32_COLOR_BLACK, COLOR_CHEESE);
```

## Laboratory 7: Collect Ingredients and Score Points

Estimated duration: 1 hour.

### Goal

Students implement game rules based on position and state.

### Activities

1. Read `collect_ingredients`.
2. Explain why an ingredient must be:
   - not already collected;
   - on the same row as the player;
   - close enough in `x`.
3. Change the points for collecting an ingredient from `25` to another value.
4. Rebuild, deploy, collect one ingredient, and verify the score.
5. Trace the all-done check that completes a pizza and resets ingredients.

### Deliverable

A rule trace for one collection event:

```text
player row = ...
ingredient row = ...
horizontal distance = ...
collected before = ...
score before = ...
score after = ...
```

### Debug Notes

If the same ingredient scores repeatedly, check whether `collected` is set. If
the pizza never completes, check the loop that searches for any ingredient still
uncollected.

### Files and Code to Add

File: `c/game.c`.

Add or study the collection routine. The comments describe collision as a rule
over stored state, not as a magic graphics operation.

```c
static void collect_ingredients(void) {
    uint8_t collected_now = 0;

    for (uint8_t i = 0; i < NUM_INGREDIENTS; ++i) {
        Ingredient *p = &ingredients[i];

        /*
         * Collection requires three facts to be true at the same time:
         * the ingredient is still active, it is on the player's row, and the
         * horizontal distance is small enough to feel like contact.
         */
        if (!p->collected &&
            p->row == player.row &&
            abs_i((player.x + 5) - p->x) < 13) {
            p->collected = 1;
            collected_now = 1;
            score += 25;
            play_tone(660 + (p->kind * 55), 45);
        }
    }

    if (!collected_now) return;

    uint8_t all_done = 1;
    for (uint8_t i = 0; i < NUM_INGREDIENTS; ++i) {
        if (!ingredients[i].collected) {
            all_done = 0;
            break;
        }
    }

    if (all_done) {
        fed_students++;
        score += 250;
        message_timer = 120;
        play_tone(988, 100);
        setup_ingredients();
        reset_enemies();
    }
}
```

For the scoring experiment, change only this line:

```c
score += 40; /* New classroom rule: each ingredient is worth 40 points. */
```

## Laboratory 8: Add Enemies, Collisions, Lives, and Game Over

Estimated duration: 1 hour.

### Goal

Students connect autonomous motion, collision detection, and state reset.

### Activities

1. Read `move_enemies`.
2. Identify how direction changes at the left and right edges.
3. Study the row-changing behavior that happens every few seconds near a
   ladder.
4. Read `check_collisions`.
5. Modify the collision threshold by a small amount and test whether the game
   feels easier or harder.
6. Verify game over and restart with `START`.

### Deliverable

A short balancing note recommending one collision threshold and explaining why.

### Concept Link

Collision detection here is intentionally simple: same row plus horizontal
distance. This prepares students for later courses where collision becomes
geometry, bounding boxes, spatial partitioning, or physics.

### Files and Code to Add

File: `c/game.c`.

Add or study the enemy update and collision functions. The first function gives
students autonomous motion; the second turns contact into consequences.

```c
static void move_enemies(void) {
    for (uint8_t i = 0; i < NUM_ENEMIES; ++i) {
        Actor *e = &enemies[i];

        /*
         * The speed expression is deliberately small and readable. It lets the
         * game become more urgent after several pizzas without introducing a
         * separate difficulty system.
         */
        int speed = 1 + (fed_students > 2) + (i == 0 && (frame_no & 1u));
        e->x += enemy_dir[i] * speed;

        /* Bounce at the edges by correcting the position and reversing sign. */
        if (e->x < 10) {
            e->x = 10;
            enemy_dir[i] = 1;
        }
        if (e->x > SCREEN_W - ENEMY_W - 10) {
            e->x = SCREEN_W - ENEMY_W - 10;
            enemy_dir[i] = -1;
        }

        e->y = row_y[e->row] - ENEMY_H;
    }
}

static void check_collisions(void) {
    for (uint8_t i = 0; i < NUM_ENEMIES; ++i) {
        Actor *e = &enemies[i];

        /*
         * Collision is intentionally conservative: same row and close centers.
         * Students can tune the threshold and immediately feel the difference.
         */
        if (e->row == player.row &&
            abs_i((player.x + PLAYER_W / 2) - (e->x + ENEMY_W / 2)) < 11) {
            if (lives > 0) lives--;
            play_tone(120, 150);

            if (lives == 0) {
                game_over = 1;
                message_timer = 255;
            }

            reset_player();
            reset_enemies();
            return;
        }
    }
}
```

For the balancing experiment, tune only the threshold:

```c
/* Smaller threshold: collision must be closer, so the game becomes easier. */
abs_i((player.x + PLAYER_W / 2) - (e->x + ENEMY_W / 2)) < 9
```

## Laboratory 9: Personalize Original Assets

Estimated duration: 1 hour.

### Goal

Students personalize the game without violating copyright or breaking the
runtime.

### Activities

1. Read `assets/original_art.md`.
2. Inspect `assets/generate_assets.py`.
3. Modify one generated visual element, such as the splash screen, background,
   or sprite colors.
4. Regenerate assets:

   ```bash
   cd assets
   python3 generate_assets.py
   cd ..
   ```

5. If the runtime should match the new asset style, update the rectangle drawing
   functions in `c/game.c`.
6. Update the asset provenance note if needed.

### Deliverable

One personalized original asset and a provenance sentence explaining who made
it and how.

### Publication Rule

Do not copy sprites, logos, music, names, level art, or sound effects from
commercial games. A classroom imitation game imitates a design problem, not
protected assets.

### Files and Code to Add

File: `assets/generate_assets.py`.

Add or modify a small visual element in the splash generator. This example is
intentionally geometric so that ownership is clear and reproducible.

```python
def draw_student_signature(draw):
    # This small mark identifies the class build without using any external
    # logo. Simple generated geometry is safe for classroom redistribution.
    draw.rectangle((238, 172, 306, 188), fill=(32, 32, 32), outline=(255, 224, 64))
    draw.text((244, 176), "CS LAB", fill=(255, 224, 64))
```

Then call it from the splash drawing function after the background and title:

```python
draw_student_signature(draw)
```

File: `assets/original_art.md`.

Add a provenance note for the modified asset:

```markdown
- Classroom splash signature: generated from rectangles and text in
  `assets/generate_assets.py` by the student team. No external image source was
  used.
```

File: `c/game.c`, only if the runtime drawing should echo the new asset.

```c
/*
 * The runtime uses rectangles rather than PNG sprites in this teaching build.
 * This small banner mirrors the generated splash signature using PRG32 calls.
 */
prg32_gfx_rect(238, 172, 68, 16, PRG32_COLOR_BLACK);
prg32_gfx_text8(244, 176, "CS LAB", COLOR_CHEESE, PRG32_COLOR_BLACK);
```

## Laboratory 10: Compare C and RISC-V Assembly

Estimated duration: 1 hour.

### Goal

Students see that the same game responsibilities can be implemented at two
levels of abstraction.

### Activities

1. Open `assembly/game.S`.
2. Find the assembly entry points:
   `you_have_got_pizza_init`, `you_have_got_pizza_update`, and
   `you_have_got_pizza_draw`.
3. Compare one C function with its assembly counterpart, preferably player
   movement or drawing.
4. Identify three common low-level operations:
   - load or store game state;
   - branch on a condition;
   - call a PRG32 runtime function.
5. Discuss why the C version is easier to modify first, and why the assembly
   version is valuable for understanding the machine.

### Deliverable

A two-column comparison of one C fragment and the corresponding assembly
fragment.

### Instructor Note

Do not require first-year students to rewrite the whole game in assembly.
Instead, ask them to explain a short fragment precisely. Correct explanation is
more valuable than blind transcription.

### Files and Code to Add

File: `assembly/game.S`.

For this laboratory, students add comments before a short assembly fragment
rather than rewriting the cartridge. The goal is to show that assembly has the
same responsibilities as the C version.

```asm
    # Teaching annotation:
    # 1. Load a value from game state.
    # 2. Compare or branch based on that value.
    # 3. Store the updated value back to memory.
    # This is the machine-level form of a C assignment inside an if statement.
```

File: lab notebook or `docs/assembly-comparison.md` in a student fork.

```markdown
| C idea | Assembly idea |
|---|---|
| `player.x += 3;` | load `player.x`, add immediate 3, store `player.x` |
| `if (input & LEFT)` | mask input bits, branch when the result is zero |
| `draw_player();` | place arguments in ABI registers, call PRG32 routine |
```

Students who want to commit the comparison can add the Markdown file to their
own fork. The reference repository keeps this laboratory as reading and
annotation so the assembly implementation remains stable.

## Laboratory 11: Metadata, Versioning, and Store Readiness

Estimated duration: 1 hour.

### Goal

Students prepare the cartridge as a publishable software artifact.

### Activities

1. Inspect `metadata/metadata.json`, `metadata/manifest.json`, and
   `metadata/colophon.json`.
2. Identify the title, version, authors, license, tags, architecture list,
   icon, screenshot, and colophon.
3. Confirm that `assets/icon.png` and `assets/screenshot.png` represent the
   current game.
4. Update the version only when the cartridge behavior or publishable content
   changes.
5. Build both architecture variants:

   ```bash
   export PRG32_REPO=$HOME/src/PRG32

   export PRG32_ARCHITECTURE=esp32c6
   scripts/build.sh

   export PRG32_ARCHITECTURE=qemu
   scripts/build.sh
   ```

6. Pack the Store bundle:

   ```bash
   scripts/pack-store-bundle.sh
   ```

### Deliverable

`dist/you-have-got-pizza-store-bundle.zip`.

### Debug Notes

If packing fails, check that both architecture-specific cartridges exist in
`dist`. The bundle script copies:

- `dist/you-have-got-pizza-esp32c6.prg32`;
- `dist/you-have-got-pizza-qemu.prg32`;
- metadata, icon, and screenshot files.

### Files and Code to Add

File: `metadata/metadata.json`.

When the cartridge behavior or public assets change, update the version and
summary carefully. This example shows the fields students are most likely to
touch.

```json
{
  "title": "You Have Got Pizza",
  "version": "1.2.0",
  "summary": "Student-customized PRG32 arcade cartridge about making pizza in an academic piazza.",
  "tags": [
    "game",
    "arcade",
    "education",
    "riscv",
    "prg32"
  ]
}
```

File: `metadata/manifest.json`.

Check that the bundle manifest points to every file the Store needs. The exact
schema may contain more fields in the repository; the pedagogical core is:

```json
{
  "id": "org.uniparthenope.prg32.you-have-got-pizza",
  "title": "You Have Got Pizza",
  "version": "1.2.0",
  "architectures": [
    {
      "id": "esp32c6",
      "file": "you-have-got-pizza-esp32c6.prg32"
    },
    {
      "id": "qemu",
      "file": "you-have-got-pizza-qemu.prg32"
    }
  ],
  "icon": "icon.png",
  "screenshot": "screenshot.png"
}
```

File: `scripts/pack-store-bundle.sh`.

The repository already includes the full script. Students should understand the
core copy step:

```bash
# A Store bundle is a ZIP containing metadata plus one cartridge per target.
cp "$repo_dir/metadata/manifest.json" "$stage_dir/manifest.json"
cp "$repo_dir/assets/icon.png" "$stage_dir/icon.png"
cp "$repo_dir/assets/screenshot.png" "$stage_dir/screenshot.png"
cp "$repo_dir/dist/$name-esp32c6.prg32" "$stage_dir/$name-esp32c6.prg32"
cp "$repo_dir/dist/$name-qemu.prg32" "$stage_dir/$name-qemu.prg32"
```

## Laboratory 12: Publish on the Cartridge Store

Estimated duration: 1 hour.

### Goal

Students publish, verify, and document the final cartridge.

### Activities

1. Confirm that the Store URL and token are available. The token may be provided
   through the environment:

   ```bash
   export PRG32_STORE_TOKEN=replace-with-the-class-token
   ```

2. Publish the bundle:

   ```bash
   PYTHONPATH="$PRG32_REPO" python3 -m prg32 store publish-bundle \
     dist/you-have-got-pizza-store-bundle.zip \
     --store-url http://192.168.1.42:5080 \
     --token "$PRG32_STORE_TOKEN"
   ```

3. Open the Cartridge Store page.
4. Verify that the listing shows:
   - title;
   - icon;
   - screenshot;
   - summary;
   - version;
   - supported architectures.
5. Install or download the cartridge from the Store if the classroom setup
   supports that workflow.

### Deliverable

A publication report containing:

- Store URL;
- cartridge title and version;
- publication time;
- one screenshot of the Store listing;
- one sentence describing what the student changed from the base game.

### Debug Notes

If publication fails with an authentication error, check the token. If the Store
accepts the upload but the listing looks wrong, inspect the metadata and asset
files, rebuild both variants, repack the bundle, and publish again.

### Files and Code to Add

File: `docs/publication-report.md` in a student fork.

Students add a short report after publication. This is not just bureaucracy: it
teaches that release engineering must leave evidence.

```markdown
# Publication Report

- Cartridge title: You Have Got Pizza
- Version: 1.2.0
- Store URL: http://192.168.1.42:5080
- Published by: student team name
- Publication time: YYYY-MM-DD HH:MM local time
- Architectures verified: ESP32-C6, QEMU
- Personal change: one sentence describing the student team's contribution

## Verification

- The Store listing shows the correct icon.
- The Store listing shows the correct screenshot.
- The Store listing offers both architecture variants.
- The cartridge starts after installation.
```

File: shell environment, not committed source.

```bash
# Keep secrets out of Git. The token belongs in the environment or local config,
# never in metadata, README files, screenshots, or lab reports.
export PRG32_STORE_TOKEN=replace-with-the-class-token
```

## Final Assessment Rubric

| Criterion | Excellent | Satisfactory | Needs work |
|---|---|---|---|
| Design clarity | Objective, rules, and feedback are clear | Main objective works | Player cannot infer rules |
| Code structure | Changes are localized and readable | Code works with minor roughness | Changes break existing structure |
| Debug discipline | Uses short code-deploy-debug cycles with notes | Tests after major steps | Makes many changes without evidence |
| Technical correctness | Build, deploy, collision, scoring, and reset all work | Core game works with small issues | Game does not run reliably |
| Originality and ethics | Assets and theme are original and documented | Mostly original with minor omissions | Uses unclear or copied assets |
| Publication quality | Store bundle is complete and verified | Bundle publishes with minor metadata issues | Bundle cannot be published |

## Suggested Extensions

After the required laboratories, stronger groups may attempt one extension:

- add a second level layout after a fixed number of pizzas;
- introduce a bonus ingredient with a timer;
- convert one rectangle sprite to a bitmap-backed sprite;
- add a difficulty setting through metadata or compile-time constants;
- port one small C function into assembly and test for identical behavior.

Extensions should follow the same rule as the core tutorial: one expected
behavior, one small edit, one build, one deployment, one observation.
