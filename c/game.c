/*
 * You Have Got Pizza - C cartridge for PRG32
 *
 * A small, original, Burger Time-inspired arcade game for the PRG32 runtime.
 * The goal is not to copy any character, level, sound, or artwork from BurgerTime:
 * the mechanics are reinterpreted as an academic Piazza where a teacher prepares
 * pizza slices for starving students.
 *
 * Visuals: PRG32 caps a cartridge at 64 KiB total (code + sprites + audio --
 * see PRG32_CART_MAX_KIB / PRG32_CART_RAM_KIB in prg32.h), so a full-screen
 * painted background would alone consume the entire budget. Instead this
 * cartridge draws small, palette-limited indexed sprites and repeats them --
 * exactly how real 8/16-bit hardware built large scenes cheaply. The pixel
 * data lives in c/assets_indexed.inc, generated from assets/png/rom/*.png by
 * assets/generate_indexed_art.py + assets/pack_indexed_assets.py, using
 * PRG32's own prg32_sprite_draw_indexed()/prg32_sprite_draw_bitplanes() API
 * (see prg32_indexed_sprite_t in prg32.h). The full-resolution reference
 * artwork in assets/png/ is generated separately by assets/generate_assets.py
 * for documentation, the Store icon, and the screenshot -- it is not compiled
 * into the cartridge.
 *
 * Audio: gameplay uses PRG32's SID-like synth mixer (four waveforms with a
 * cutoff/resonance filter, per-instrument ADSR, 8 voices, stereo panning).
 * assets/generate_audio.py writes assets/audio.json, which PRG32's
 * tools/prg32audio_pack.py packs into an AUDIO block embedded in the
 * cartridge (see scripts/build.sh --audio-block); the block auto-loads when
 * the cartridge installs, so this file only has to trigger notes. Channels
 * 0-4 carry a looping 5-voice background theme (prg32_audio_play_track);
 * channels 5-7 are reserved for one-shot sound effects so gameplay stingers
 * never steal a voice from the music.
 *
 * PRG32 asks a cartridge to export exactly three functions. The cartridge builder
 * finds them from the entry prefix passed on the command line:
 *
 * you_have_got_pizza_c_init
 * you_have_got_pizza_c_update
 * you_have_got_pizza_c_draw
 *
 * The update function changes the model. The draw function renders the model.
 * Keeping those responsibilities separate is a very useful habit when students
 * later move from C to assembly, because it reduces the number of live variables
 * that must be followed at one time.
 */

#include "prg32.h"
#include <stdint.h>
#include "assets_indexed.inc"

#define SCREEN_W 320
#define SCREEN_H 200
#define PLAYER_W 10
#define PLAYER_H 14
#define ENEMY_W 10
#define ENEMY_H 10
#define NUM_ROWS 4
#define NUM_LADDERS 4
#define NUM_INGREDIENTS 8
#define NUM_ENEMIES 4
#define MAX_LIVES 3

#define COLOR_CHEESE    0xffe0

#define BTN_MOVE_MASK (PRG32_BTN_LEFT | PRG32_BTN_RIGHT | PRG32_BTN_UP | PRG32_BTN_DOWN)

/* SID-like instrument ids, matching the order registered in assets/audio.json.
 * Channels 0-4 are the looping background theme (instrument id == channel,
 * enforced by the tracker); channels 5-7 are free for one-shot effects. */
#define INSTR_BASS   0
#define INSTR_PAD_A  1
#define INSTR_PAD_B  2
#define INSTR_ARP    3
#define INSTR_HAT    4
#define INSTR_BLIP   5
#define INSTR_CHIME  6
#define INSTR_THUD   7
#define SFX_CHANNEL_FIRST 5
#define SFX_CHANNEL_LAST  7

typedef struct {
    int16_t x;
    int16_t y;
    int8_t row;
    int8_t alive;
} Actor;

typedef struct {
    int16_t x;
    int8_t row;
    uint8_t kind;
    uint8_t collected;
} Ingredient;

static const int16_t row_y[NUM_ROWS] = { 48, 88, 128, 168 };
static const int16_t ladder_x[NUM_LADDERS] = { 32, 104, 188, 272 };
static const char *const kind_name[4] = { "DOUGH", "SAUCE", "CHEESE", "BASIL" };

/* One-shot sound effects round-robin across channels 5-7 so a burst of
 * simultaneous events (e.g. a collect right before a collision) never
 * silently steals a voice from another in-flight effect. The 5-voice
 * background theme on channels 0-4 is never touched. */
static uint8_t sfx_channel = SFX_CHANNEL_FIRST;

static void sfx(uint8_t instrument, uint8_t note, uint8_t volume,
                uint32_t duration_ms, int8_t pan) {
    uint8_t channel = sfx_channel;
    sfx_channel = (sfx_channel >= SFX_CHANNEL_LAST) ? SFX_CHANNEL_FIRST
                                                     : (uint8_t)(sfx_channel + 1);
    prg32_audio_set_channel_pan(channel, pan);
    prg32_audio_note(channel, instrument, note, volume, duration_ms);
}

static Actor player;
static Actor enemies[NUM_ENEMIES];
static int8_t enemy_dir[NUM_ENEMIES];
static Ingredient ingredients[NUM_INGREDIENTS];
static uint32_t frame_no;
static uint16_t score;
static uint8_t lives;
static uint8_t fed_students;
static uint8_t message_timer;
static uint8_t game_over;

static int abs_i(int v) { return v < 0 ? -v : v; }

static void draw_text_num2(int x, int y, uint16_t value, uint16_t fg) {
    char s[3];
    value %= 100;
    s[0] = (char)('0' + value / 10);
    s[1] = (char)('0' + value % 10);
    s[2] = 0;
    prg32_gfx_text8(x, y, s, fg, PRG32_COLOR_BLACK);
}

static void draw_text_num4(int x, int y, uint16_t value, uint16_t fg) {
    char s[5];
    value %= 10000;
    s[0] = (char)('0' + (value / 1000) % 10);
    s[1] = (char)('0' + (value / 100) % 10);
    s[2] = (char)('0' + (value / 10) % 10);
    s[3] = (char)('0' + value % 10);
    s[4] = 0;
    prg32_gfx_text8(x, y, s, fg, PRG32_COLOR_BLACK);
}

static void setup_ingredients(void) {
    static const int16_t x[NUM_INGREDIENTS] = { 60, 132, 220, 276, 84, 164, 244, 116 };
    static const int8_t r[NUM_INGREDIENTS] = { 0, 0, 0, 1, 2, 2, 3, 3 };
    for (uint8_t i = 0; i < NUM_INGREDIENTS; ++i) {
        ingredients[i].x = x[i];
        ingredients[i].row = r[i];
        ingredients[i].kind = i & 3u;
        ingredients[i].collected = 0;
    }
}

static void reset_player(void) {
    player.x = 24;
    player.row = 3;
    player.y = row_y[player.row] - PLAYER_H;
    player.alive = 1;
}

static void reset_enemies(void) {
    static const int16_t ex[NUM_ENEMIES] = { 280, 48, 212, 140 };
    static const int8_t er[NUM_ENEMIES] = { 3, 2, 1, 0 };
    for (uint8_t i = 0; i < NUM_ENEMIES; ++i) {
        enemies[i].x = ex[i];
        enemies[i].row = er[i];
        enemies[i].y = row_y[er[i]] - ENEMY_H;
        enemies[i].alive = 1;
        enemy_dir[i] = (i & 1u) ? 1 : -1;
    }
}

static void start_new_game(void) {
    frame_no = 0;
    score = 0;
    lives = MAX_LIVES;
    fed_students = 0;
    message_timer = 90;
    game_over = 0;
    setup_ingredients();
    reset_player();
    reset_enemies();
}

static int nearest_ladder_index(int x) {
    int best = 0;
    int best_d = 999;
    for (int i = 0; i < NUM_LADDERS; ++i) {
        int d = abs_i(x - ladder_x[i]);
        if (d < best_d) {
            best_d = d;
            best = i;
        }
    }
    return best_d <= 9 ? best : -1;
}

static void move_player(uint32_t input) {
    if (input & PRG32_BTN_LEFT) {
        player.x -= 3;
    }
    if (input & PRG32_BTN_RIGHT) {
        player.x += 3;
    }
    if (player.x < 8) player.x = 8;
    if (player.x > SCREEN_W - PLAYER_W - 8) player.x = SCREEN_W - PLAYER_W - 8;

    int ladder = nearest_ladder_index(player.x + PLAYER_W / 2);
    if (ladder >= 0) {
        /* FIXED: Only snap to ladder and shift rows if UP or DOWN is actively pressed */
        if ((input & PRG32_BTN_UP) && player.row > 0) {
            player.x = ladder_x[ladder] - PLAYER_W / 2;
            player.row--;
            sfx(INSTR_CHIME, 69, 150, 60, PRG32_AUDIO_PAN_CENTER);
        } else if ((input & PRG32_BTN_DOWN) && player.row < NUM_ROWS - 1) {
            player.x = ladder_x[ladder] - PLAYER_W / 2;
            player.row++;
            sfx(INSTR_CHIME, 62, 150, 60, PRG32_AUDIO_PAN_CENTER);
        }
    }
    player.y = row_y[player.row] - PLAYER_H;
}

static void collect_ingredients(void) {
    uint8_t collected_now = 0;
    for (uint8_t i = 0; i < NUM_INGREDIENTS; ++i) {
        Ingredient *p = &ingredients[i];
        if (!p->collected && p->row == player.row && abs_i((player.x + 5) - p->x) < 13) {
            p->collected = 1;
            collected_now = 1;
            score += 25;
            int8_t pan = (int8_t)(-40 + p->kind * 27);
            sfx(INSTR_BLIP, (uint8_t)(72 + p->kind * 3), 220, 90, pan);
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
        sfx(INSTR_CHIME, 84, 255, 260, PRG32_AUDIO_PAN_CENTER);
        setup_ingredients();
        reset_enemies();
    }
}

static void move_enemies(void) {
    for (uint8_t i = 0; i < NUM_ENEMIES; ++i) {
        Actor *e = &enemies[i];
        int speed = 1 + (fed_students > 2) + (i == 0 && (frame_no & 1u));
        e->x += enemy_dir[i] * speed;
        if (e->x < 10) {
            e->x = 10;
            enemy_dir[i] = 1;
        }
        if (e->x > SCREEN_W - ENEMY_W - 10) {
            e->x = SCREEN_W - ENEMY_W - 10;
            enemy_dir[i] = -1;
        }

        /* Every few seconds a student finds a staircase and changes row. */
        if (((frame_no + i * 37u) % 180u) == 0u) {
            int ladder = nearest_ladder_index(e->x + ENEMY_W / 2);
            if (ladder >= 0) {
                if ((i + frame_no) & 1u) {
                    if (e->row > 0) e->row--;
                } else {
                    if (e->row < NUM_ROWS - 1) e->row++;
                }
            }
        }
        e->y = row_y[e->row] - ENEMY_H;
    }
}

static void check_collisions(void) {
    for (uint8_t i = 0; i < NUM_ENEMIES; ++i) {
        Actor *e = &enemies[i];
        if (e->row == player.row && abs_i((player.x + PLAYER_W / 2) - (e->x + ENEMY_W / 2)) < 11) {
            if (lives > 0) lives--;
            if (lives == 0) {
                game_over = 1;
                message_timer = 255;
                sfx(INSTR_THUD, 28, 255, 400, PRG32_AUDIO_PAN_CENTER);
            } else {
                sfx(INSTR_THUD, 43, 220, 160, PRG32_AUDIO_PAN_CENTER);
            }
            reset_player();
            reset_enemies();
            return;
        }
    }
}

static void draw_piazza(void) {
    /* Sky gradient: three cheap flat bands read as atmosphere at this scale,
     * for free -- no sprite data needed for open sky. */
    prg32_gfx_rect(0, 0, SCREEN_W, 24, 0x6bdf);
    prg32_gfx_rect(0, 24, SCREEN_W, 10, 0x7bff);
    prg32_gfx_rect(0, 34, SCREEN_W, SCREEN_H - 34, 0x867f);

    /* Skyline: one painted arch tile (bitplane-packed) repeated across the
     * width instead of a full-width bitmap. */
    for (int x = 16; x < SCREEN_W; x += 40) {
        prg32_sprite_draw_bitplanes(x, 8, &art_tile_arch, 0);
    }

    /* Platforms: one painted stone tile repeated along each row. */
    for (uint8_t r = 0; r < NUM_ROWS; ++r) {
        int y = row_y[r];
        for (int x = 8; x < 312; x += ART_TILE_STONE_W) {
            prg32_sprite_draw_indexed(x, y, &art_tile_stone, 0);
        }
    }

    /* Ladders: one painted rung tile repeated vertically. */
    for (uint8_t i = 0; i < NUM_LADDERS; ++i) {
        int x = ladder_x[i] - ART_TILE_LADDER_W / 2;
        for (int y = row_y[0]; y < row_y[3] + 8; y += ART_TILE_LADDER_H) {
            prg32_sprite_draw_indexed(x, y, &art_tile_ladder, 0);
        }
    }

    /* The plate at the bottom is the destination for every completed pizza. */
    prg32_sprite_draw_indexed(256 - ART_TILE_PLATE_W / 2, SCREEN_H - ART_TILE_PLATE_H,
                              &art_tile_plate, 0);
}

static void draw_ingredient(const Ingredient *p) {
    int y = row_y[p->row] - 12;
    int x = p->x - ART_INGREDIENTS_W / 2;
    if (p->collected) {
        prg32_gfx_text8(p->x - 12, y - 8, "OK", PRG32_COLOR_WHITE, PRG32_COLOR_BLACK);
        return;
    }
    uint8_t anim = (uint8_t)((frame_no >> 4) & 3u);
    uint32_t frame = (uint32_t)p->kind * 4u + anim;
    prg32_sprite_draw_indexed(x, y, &art_ingredients, frame);
}

static void draw_player(void) {
    /* Four-frame animated indexed sprite (see assets/png/rom and
     * c/assets_indexed.inc). The sprite is a couple of pixels larger than
     * the PLAYER_W/PLAYER_H hitbox, so it is anchored to the hitbox's
     * bottom-center -- the collision box itself is unchanged. */
    int x = player.x + (PLAYER_W - ART_PROFESSOR_W) / 2;
    int y = player.y + PLAYER_H - ART_PROFESSOR_H;
    uint8_t anim = (uint8_t)((frame_no >> 3) & 3u);
    prg32_sprite_draw_indexed(x, y, &art_professor, anim);
}

static void draw_enemy(const Actor *e, uint8_t i) {
    const prg32_indexed_sprite_t *sheet = (i & 1u) ? &art_student_magenta : &art_student_blue;
    uint8_t anim = (uint8_t)(((frame_no >> 3) + i) & 3u);
    int x = e->x + (ENEMY_W - ART_STUDENT_BLUE_W) / 2;
    int y = e->y + ENEMY_H - ART_STUDENT_BLUE_H;
    prg32_sprite_draw_indexed(x, y, sheet, anim);
}

static void draw_hud(void) {
    prg32_gfx_rect(0, 0, 320, 16, PRG32_COLOR_BLACK);
    prg32_gfx_text8(4, 4, "YOU HAVE GOT PIZZA", COLOR_CHEESE, PRG32_COLOR_BLACK);
    prg32_gfx_text8(172, 4, "SCORE", PRG32_COLOR_WHITE, PRG32_COLOR_BLACK);
    draw_text_num4(224, 4, score, PRG32_COLOR_WHITE);
    for (uint8_t i = 0; i < lives; ++i) {
        prg32_sprite_draw_indexed(272 + i * (ART_LIFE_ICON_W + 3), 4, &art_life_icon, 0);
    }
}

static void draw_recipe_hint(void) {
    prg32_gfx_rect(4, 184, 205, 13, PRG32_COLOR_BLACK);
    prg32_gfx_text8(8, 187, "Recipe: dough sauce cheese basil", COLOR_CHEESE, PRG32_COLOR_BLACK);
}

void you_have_got_pizza_c_init(void) {
    /* Start the looping SID-like theme once; a later restart (see update()
     * below) resets gameplay state only, so the music keeps playing through
     * a "press START to retry". Mono-vs-stereo output is a resident-firmware
     * build setting (prg32_audio_set_mode is not in the portable cartridge
     * ABI table -- a cartridge cannot force it), so the channel panning here
     * is what actually carries the stereo image once the firmware is built
     * with stereo output enabled; it still shapes the mix under mono fold-down. */
    prg32_audio_play_track(0);
    start_new_game();
}

void you_have_got_pizza_c_update(void) {
    frame_no++;
    uint32_t input = prg32_input_read();

    if (game_over) {
        if (input & PRG32_BTN_START) {
            start_new_game();
        }
        return;
    }

    if (message_timer > 0) message_timer--;

    move_player(input & BTN_MOVE_MASK);
    collect_ingredients();
    move_enemies();
    check_collisions();
}

void you_have_got_pizza_c_draw(void) {
    draw_piazza();
    for (uint8_t i = 0; i < NUM_INGREDIENTS; ++i) draw_ingredient(&ingredients[i]);
    for (uint8_t i = 0; i < NUM_ENEMIES; ++i) draw_enemy(&enemies[i], i);
    draw_player();
    draw_hud();
    draw_recipe_hint();

    if (message_timer > 0) {
        if (game_over) {
            prg32_gfx_rect(52, 68, 216, 40, PRG32_COLOR_BLACK);
            prg32_gfx_text8(88, 78, "GAME OVER - PRESS START", PRG32_COLOR_RED, PRG32_COLOR_BLACK);
            prg32_gfx_text8(92, 94, "STUDENTS STILL HUNGRY", COLOR_CHEESE, PRG32_COLOR_BLACK);
        } else if (fed_students == 0 && score == 0) {
            /* Attract screen: the one place with room in the 64 KiB budget
             * for a bigger painted centerpiece (see art_title_emblem). */
            prg32_gfx_rect(40, 56, 240, 66, PRG32_COLOR_BLACK);
            prg32_sprite_draw_indexed(48, 62, &art_title_emblem, 0);
            prg32_gfx_text8(114, 70, "Feed students", COLOR_CHEESE, PRG32_COLOR_BLACK);
            prg32_gfx_text8(114, 84, "with pizza!", COLOR_CHEESE, PRG32_COLOR_BLACK);
            prg32_gfx_text8(114, 104, "Use joystick 1 only", PRG32_COLOR_WHITE, PRG32_COLOR_BLACK);
        } else {
            prg32_gfx_rect(52, 68, 216, 40, PRG32_COLOR_BLACK);
            prg32_gfx_text8(80, 78, "A PIZZA IS READY!", COLOR_CHEESE, PRG32_COLOR_BLACK);
            prg32_gfx_text8(68, 94, "Cool tools make hunger", PRG32_COLOR_WHITE, PRG32_COLOR_BLACK);
        }
    }
}