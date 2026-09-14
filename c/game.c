/*
 * You Have Got Pizza - C cartridge for PRG32
 *
 * A small, original, Burger Time-inspired arcade game for the PRG32 runtime.
 * The goal is not to copy any character, level, sound, or artwork from BurgerTime:
 * the mechanics are reinterpreted as an academic Piazza where a teacher prepares
 * pizza slices for starving students. All visuals are generated from rectangles
 * and tiny original data tables in this file. The repository also includes
 * copyright-clean PNG sprite sheets and WAV masters under assets/; the current
 * cartridge uses rectangle sprites and PRG32 timed notes so it remains small and easy
 * to study in assembly.
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

#define COLOR_SKY       0x867f
#define COLOR_STONE     0x8410
#define COLOR_STONE_DK  0x4208
#define COLOR_PIAZZA    0xc638
#define COLOR_DOUGH     0xff16
#define COLOR_SAUCE     0xd000
#define COLOR_CHEESE    0xffe0
#define COLOR_BASIL     0x0480
#define COLOR_STUDENT   0x7bef
#define COLOR_PROF      0xfbe0
#define COLOR_SHADOW    0x2104

#define BTN_MOVE_MASK (PRG32_BTN_LEFT | PRG32_BTN_RIGHT | PRG32_BTN_UP | PRG32_BTN_DOWN)

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

/* Keep short classroom tones on the portable, MIDI-note audio API. */
static void play_tone(uint32_t hz, uint32_t duration_ms) {
    static const uint16_t semitone_hz[13] = {
        262, 277, 294, 311, 330, 349, 370, 392, 415, 440, 466, 494, 523
    };
    int octave = 4;
    while (hz < 262 && octave > 0) { hz *= 2; octave--; }
    while (hz >= 523 && octave < 8) { hz /= 2; octave++; }
    uint8_t semitone = 0;
    while (semitone < 12 &&
           hz > (uint32_t)(semitone_hz[semitone] + semitone_hz[semitone + 1]) / 2u) {
        semitone++;
    }
    prg32_audio_note(0, PRG32_DEFAULT_INSTRUMENT_ID,
                     (uint8_t)(12 * (octave + 1) + semitone), 190, duration_ms);
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

static uint16_t ingredient_color(uint8_t kind) {
    switch (kind & 3u) {
    case 0: return COLOR_DOUGH;
    case 1: return COLOR_SAUCE;
    case 2: return COLOR_CHEESE;
    default: return COLOR_BASIL;
    }
}

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
            play_tone(330, 18);
        } else if ((input & PRG32_BTN_DOWN) && player.row < NUM_ROWS - 1) {
            player.x = ladder_x[ladder] - PLAYER_W / 2;
            player.row++;
            play_tone(220, 18);
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

static void draw_piazza(void) {
    prg32_gfx_clear(COLOR_SKY);

    /* Skyline and university arches. */
    prg32_gfx_rect(0, 24, 320, 26, 0x632c);
    for (int x = 16; x < 320; x += 40) {
        prg32_gfx_rect(x, 8, 18, 42, 0x8430);
        prg32_gfx_rect(x + 5, 18, 8, 16, PRG32_COLOR_BLACK);
    }

    /* Platforms are the steps of the Piazza. */
    for (uint8_t r = 0; r < NUM_ROWS; ++r) {
        int y = row_y[r];
        prg32_gfx_rect(8, y, 304, 5, COLOR_STONE_DK);
        prg32_gfx_rect(8, y + 5, 304, 5, COLOR_STONE);
        for (int x = 12; x < 304; x += 16) {
            prg32_gfx_rect(x, y + 6, 1, 3, COLOR_SHADOW);
        }
    }

    /* Ladders are safe ways through the crowd. */
    for (uint8_t i = 0; i < NUM_LADDERS; ++i) {
        int x = ladder_x[i];
        prg32_gfx_rect(x - 6, row_y[0], 3, row_y[3] - row_y[0] + 8, COLOR_STONE_DK);
        prg32_gfx_rect(x + 5, row_y[0], 3, row_y[3] - row_y[0] + 8, COLOR_STONE_DK);
        for (int y = row_y[0] + 6; y < row_y[3] + 8; y += 12) {
            prg32_gfx_rect(x - 6, y, 14, 3, COLOR_STONE);
        }
    }

    /* The plate at the bottom is the destination for every completed pizza. */
    prg32_gfx_rect(218, 186, 76, 6, PRG32_COLOR_WHITE);
    prg32_gfx_rect(230, 181, 52, 5, COLOR_DOUGH);
}

static void draw_ingredient(const Ingredient *p) {
    int y = row_y[p->row] - 12;
    uint16_t c = p->collected ? COLOR_STONE : ingredient_color(p->kind);
    prg32_gfx_rect(p->x - 13, y, 26, 8, c);
    prg32_gfx_rect(p->x - 10, y + 2, 5, 4, PRG32_COLOR_WHITE);
    prg32_gfx_rect(p->x + 5, y + 2, 5, 4, PRG32_COLOR_WHITE);
    if (p->collected) {
        prg32_gfx_text8(p->x - 12, y - 8, "OK", PRG32_COLOR_WHITE, PRG32_COLOR_BLACK);
    }
}

static void draw_player(void) {
    int x = player.x;
    int y = player.y;

    /*
     * Animated sprite, drawn from small rectangles.
     *
     * PRG32 can draw bitmap assets when a project chooses to convert PNGs into
     * RGB565 tables, but rectangle sprites are perfect for a first RISC-V lab:
     * every body part is just a function call with five integer arguments.  The
     * frame number selects one of four poses, matching the master PNG sheet in
     * assets/png/sprite_professor_4frames_12x16.png.
     */
    uint8_t anim = (uint8_t)((frame_no >> 3) & 3u);
    uint8_t long_left_leg = (anim == 0u || anim == 3u);

    prg32_gfx_rect(x + 2, y, 6, 4, COLOR_PROF);
    prg32_gfx_rect(x + 3, y + 1, 1, 1, PRG32_COLOR_BLACK);
    prg32_gfx_rect(x + 6, y + 1, 1, 1, PRG32_COLOR_BLACK);
    prg32_gfx_rect(x + 1, y + 4, 8, 7, PRG32_COLOR_BLUE);
    prg32_gfx_rect(x + 4, y + 5, 2, 6, PRG32_COLOR_WHITE);

    if (anim & 1u) {
        prg32_gfx_rect(x, y + 7, 2, 4, COLOR_PROF);
        prg32_gfx_rect(x + 8, y + 6, 2, 4, COLOR_PROF);
    } else {
        prg32_gfx_rect(x, y + 6, 2, 4, COLOR_PROF);
        prg32_gfx_rect(x + 8, y + 7, 2, 4, COLOR_PROF);
    }

    prg32_gfx_rect(x + 1, y + 11, 3, long_left_leg ? 3 : 2, COLOR_SHADOW);
    prg32_gfx_rect(x + 6, y + 11, 3, long_left_leg ? 2 : 3, COLOR_SHADOW);
}

static void draw_enemy(const Actor *e, uint8_t i) {
    uint16_t shirt = (i & 1u) ? PRG32_COLOR_MAGENTA : COLOR_STUDENT;
    uint8_t anim = (uint8_t)(((frame_no >> 3) + i) & 3u);

    /* Hungry students use the same four-frame walk cycle idea as the professor. */
    prg32_gfx_rect(e->x + 2, e->y, 6, 4, 0xffbe);
    prg32_gfx_rect(e->x, e->y + 4, 10, 6, shirt);
    prg32_gfx_rect(e->x + 2, e->y + 2, 1, 1, PRG32_COLOR_BLACK);
    prg32_gfx_rect(e->x + 7, e->y + 2, 1, 1, PRG32_COLOR_BLACK);

    if (anim & 1u) {
        prg32_gfx_rect(e->x, e->y + 5, 2, 2, 0xffbe);
        prg32_gfx_rect(e->x + 9, e->y + 7, 1, 2, 0xffbe);
        prg32_gfx_rect(e->x + 2, e->y + 10, 3, 4, COLOR_SHADOW);
        prg32_gfx_rect(e->x + 7, e->y + 10, 3, 2, COLOR_SHADOW);
    } else {
        prg32_gfx_rect(e->x, e->y + 7, 2, 2, 0xffbe);
        prg32_gfx_rect(e->x + 9, e->y + 5, 1, 2, 0xffbe);
        prg32_gfx_rect(e->x + 2, e->y + 10, 3, 2, COLOR_SHADOW);
        prg32_gfx_rect(e->x + 7, e->y + 10, 3, 4, COLOR_SHADOW);
    }
}

static void draw_hud(void) {
    prg32_gfx_rect(0, 0, 320, 16, PRG32_COLOR_BLACK);
    prg32_gfx_text8(4, 4, "YOU HAVE GOT PIZZA", COLOR_CHEESE, PRG32_COLOR_BLACK);
    prg32_gfx_text8(172, 4, "SCORE", PRG32_COLOR_WHITE, PRG32_COLOR_BLACK);
    draw_text_num4(224, 4, score, PRG32_COLOR_WHITE);
    prg32_gfx_text8(268, 4, "L", PRG32_COLOR_WHITE, PRG32_COLOR_BLACK);
    draw_text_num2(284, 4, lives, PRG32_COLOR_WHITE);
}

static void draw_recipe_hint(void) {
    prg32_gfx_rect(4, 184, 205, 13, PRG32_COLOR_BLACK);
    prg32_gfx_text8(8, 187, "Recipe: dough sauce cheese basil", COLOR_CHEESE, PRG32_COLOR_BLACK);
}

void you_have_got_pizza_c_init(void) {
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
        prg32_gfx_rect(52, 68, 216, 40, PRG32_COLOR_BLACK);
        if (game_over) {
            prg32_gfx_text8(88, 78, "GAME OVER - PRESS START", PRG32_COLOR_RED, PRG32_COLOR_BLACK);
            prg32_gfx_text8(92, 94, "STUDENTS STILL HUNGRY", COLOR_CHEESE, PRG32_COLOR_BLACK);
        } else if (fed_students == 0 && score == 0) {
            prg32_gfx_text8(72, 78, "Feed students with pizza!", COLOR_CHEESE, PRG32_COLOR_BLACK);
            prg32_gfx_text8(76, 94, "Use joystick 1 only", PRG32_COLOR_WHITE, PRG32_COLOR_BLACK);
        } else {
            prg32_gfx_text8(80, 78, "A PIZZA IS READY!", COLOR_CHEESE, PRG32_COLOR_BLACK);
            prg32_gfx_text8(68, 94, "Cool tools make hunger", PRG32_COLOR_WHITE, PRG32_COLOR_BLACK);
        }
    }
}