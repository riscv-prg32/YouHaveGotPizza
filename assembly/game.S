/*
 * You Have Got Pizza - RISC-V assembly cartridge for PRG32
 *
 * Pedagogical note
 * ----------------
 * This file is deliberately written in small, traceable pieces.  The program is
 * a real game, but every routine also has a teaching role: students can inspect
 * one loop, one branch, or one ABI call without first understanding a large game
 * engine.  The game uses only original rectangle art and short beeps.
 *
 * PRG32 cartridge ABI
 * -------------------
 * The cartridge builder is invoked with:
 *
 *   --entry-prefix you_have_got_pizza
 *
 * Therefore the runtime will call these three exported labels:
 *
 *   you_have_got_pizza_init
 *   you_have_got_pizza_update
 *   you_have_got_pizza_draw
 *
 * Each PRG32 service follows the normal RISC-V C ABI: arguments in a0..a7,
 * return value in a0, caller-saved temporaries in t0..t6.
 */

.option norelax

/* Button bit masks from prg32.h.  Only joystick 1 is read by prg32_input_read. */
.equ BTN_LEFT,   0x01
.equ BTN_RIGHT,  0x02
.equ BTN_UP,     0x04
.equ BTN_DOWN,   0x08
.equ BTN_START,  0x40

/* RGB565 colors. */
.equ BLACK,      0x0000
.equ WHITE,      0xffff
.equ RED,        0xf800
.equ GREEN,      0x07e0
.equ BLUE,       0x001f
.equ YELLOW,     0xffe0
.equ CYAN,       0x07ff
.equ MAGENTA,    0xf81f
.equ SKY,        0x867f
.equ STONE,      0x8410
.equ STONE_DK,   0x4208
.equ DOUGH,      0xff16
.equ SAUCE,      0xd000
.equ BASIL,      0x0480
.equ SHADOW,     0x2104

.equ NUM_ING,    8
.equ NUM_ENEMY,  4
.equ PLAYER_W,   10
.equ PLAYER_H,   14
.equ ENEMY_W,    10
.equ ENEMY_H,    10

.section .rodata
row_y:          .half 48, 88, 128, 168
ladder_x:       .half 32, 104, 188, 272
ing_init_x:     .half 60, 132, 220, 276, 84, 164, 244, 116
ing_init_row:   .byte 0, 0, 0, 1, 2, 2, 3, 3
ing_kind:       .byte 0, 1, 2, 3, 0, 1, 2, 3
enemy_init_x:   .half 280, 48, 212, 140
enemy_init_row: .byte 3, 2, 1, 0
enemy_init_dir: .byte -1, 1, -1, 1

title_txt:      .asciz "YOU HAVE GOT PIZZA"
hint_txt:       .asciz "Collect: dough sauce cheese basil"
start_txt:      .asciz "Piazza edition - Joystick 1"
ready_txt:      .asciz "A PIZZA IS READY!"
over_txt:       .asciz "GAME OVER - PRESS START"
score_txt:      .asciz "SCORE"
lives_txt:      .asciz "LIVES"

.section .bss
.balign 4
player_x:       .word 0
player_row:     .word 0
player_y:       .word 0
score:          .word 0
lives:          .word 0
fed_students:   .word 0
frame_no:       .word 0
message_timer:  .word 0
game_over:      .word 0
ing_x:          .space NUM_ING * 4
ing_row:        .space NUM_ING
ing_collected:  .space NUM_ING
enemy_x:        .space NUM_ENEMY * 4
enemy_row:      .space NUM_ENEMY
enemy_dir:      .space NUM_ENEMY

.section .text
.global you_have_got_pizza_init
.global you_have_got_pizza_update
.global you_have_got_pizza_draw

/* Convert the five classroom sound frequencies to nearby MIDI pitches.
   Input: a0 = frequency in Hz, a1 = duration in milliseconds.
   The wrapper preserves ra and keeps the stack aligned for the C ABI call. */
pizza_audio_note:
    addi    sp, sp, -16
    sw      ra, 12(sp)
    mv      a4, a1
    li      t0, 170
    blt     a0, t0, .L_tone_low
    li      t0, 275
    blt     a0, t0, .L_tone_a3
    li      t0, 495
    blt     a0, t0, .L_tone_e4
    li      t0, 825
    blt     a0, t0, .L_tone_e5
    li      a2, 83              /* B5, near 988 Hz */
    j       .L_tone_call
.L_tone_low:
    li      a2, 48              /* C3, near 120 Hz */
    j       .L_tone_call
.L_tone_a3:
    li      a2, 57              /* A3, 220 Hz */
    j       .L_tone_call
.L_tone_e4:
    li      a2, 64              /* E4, 330 Hz */
    j       .L_tone_call
.L_tone_e5:
    li      a2, 76              /* E5, 660 Hz */
.L_tone_call:
    li      a0, 0               /* audio channel */
    li      a1, 0               /* default instrument */
    li      a3, 190             /* volume */
    call    prg32_audio_note
    lw      ra, 12(sp)
    addi    sp, sp, 16
    ret

/* ------------------------------------------------------------
 * Helper: row_to_y
 * Input:  a0 = row index 0..3
 * Output: a0 = screen y coordinate for the top-left corner of player body
 * Teaching point: table lookup = base address + scaled index.
 * ------------------------------------------------------------ */
row_to_y:
    la      t0, row_y
    slli    t1, a0, 1              /* halfword index: row * 2 */
    add     t0, t0, t1
    lhu     a0, 0(t0)
    addi    a0, a0, -PLAYER_H
    ret

/* Helper: abs_t0.  Input t0, output t0 = absolute value. */
abs_t0:
    bge     t0, zero, 1f
    neg     t0, t0
1:  ret

/* Helper: color_from_kind. Input a0=kind 0..3, output a0=color. */
color_from_kind:
    beqz    a0, 1f
    li      t0, 1
    beq     a0, t0, 2f
    li      t0, 2
    beq     a0, t0, 3f
    li      a0, BASIL
    ret
1:  li      a0, DOUGH
    ret
2:  li      a0, SAUCE
    ret
3:  li      a0, YELLOW
    ret

/* Helper: reset_player.  It calls row_to_y, so it preserves ra. */
reset_player:
    addi    sp, sp, -16
    sw      ra, 12(sp)
    li      t0, 24
    la      t1, player_x
    sw      t0, 0(t1)
    li      t0, 3
    la      t1, player_row
    sw      t0, 0(t1)
    mv      a0, t0
    call    row_to_y
    la      t1, player_y
    sw      a0, 0(t1)
    lw      ra, 12(sp)
    addi    sp, sp, 16
    ret

/* Helper: reset_enemies. Copies small ROM tables to mutable state arrays. */
reset_enemies:
    li      t0, 0
1:
    li      t6, NUM_ENEMY
    bge     t0, t6, 2f
    la      t1, enemy_init_x
    slli    t2, t0, 1
    add     t1, t1, t2
    lhu     t3, 0(t1)
    la      t4, enemy_x
    slli    t5, t0, 2
    add     t4, t4, t5
    sw      t3, 0(t4)

    la      t1, enemy_init_row
    add     t1, t1, t0
    lbu     t3, 0(t1)
    la      t4, enemy_row
    add     t4, t4, t0
    sb      t3, 0(t4)

    la      t1, enemy_init_dir
    add     t1, t1, t0
    lb      t3, 0(t1)
    la      t4, enemy_dir
    add     t4, t4, t0
    sb      t3, 0(t4)

    addi    t0, t0, 1
    j       1b
2:  ret

/* Helper: setup_ingredients. */
setup_ingredients:
    li      t0, 0
1:
    li      t6, NUM_ING
    bge     t0, t6, 2f
    la      t1, ing_init_x
    slli    t2, t0, 1
    add     t1, t1, t2
    lhu     t3, 0(t1)
    la      t4, ing_x
    slli    t5, t0, 2
    add     t4, t4, t5
    sw      t3, 0(t4)

    la      t1, ing_init_row
    add     t1, t1, t0
    lbu     t3, 0(t1)
    la      t4, ing_row
    add     t4, t4, t0
    sb      t3, 0(t4)

    la      t4, ing_collected
    add     t4, t4, t0
    sb      zero, 0(t4)
    addi    t0, t0, 1
    j       1b
2:  ret

/* ------------------------------------------------------------
 * Init: reset every variable to a reproducible state.
 * ------------------------------------------------------------ */
you_have_got_pizza_init:
    addi    sp, sp, -16
    sw      ra, 12(sp)
    la      t0, score
    sw      zero, 0(t0)
    la      t0, fed_students
    sw      zero, 0(t0)
    la      t0, frame_no
    sw      zero, 0(t0)
    la      t0, game_over
    sw      zero, 0(t0)
    li      t1, 3
    la      t0, lives
    sw      t1, 0(t0)
    li      t1, 120
    la      t0, message_timer
    sw      t1, 0(t0)
    call    setup_ingredients
    call    reset_player
    call    reset_enemies
    lw      ra, 12(sp)
    addi    sp, sp, 16
    ret

/* Helper: test if player is on a ladder. Output a0=1 if yes, a0=0 if no. */
player_on_ladder:
    addi    sp, sp, -16
    sw      ra, 12(sp)
    la      t0, player_x
    lw      t1, 0(t0)
    addi    t1, t1, 5              /* player center */
    li      t2, 0
1:
    li      t6, 4
    bge     t2, t6, 3f
    la      t3, ladder_x
    slli    t4, t2, 1
    add     t3, t3, t4
    lhu     t5, 0(t3)
    sub     t0, t1, t5
    call    abs_t0
    li      t6, 10
    blt     t0, t6, 2f
    addi    t2, t2, 1
    j       1b
2:  addi    t5, t5, -5
    la      t3, player_x
    sw      t5, 0(t3)              /* snap to ladder: movement becomes readable */
    li      a0, 1
    lw      ra, 12(sp)
    addi    sp, sp, 16
    ret
3:  li      a0, 0
    lw      ra, 12(sp)
    addi    sp, sp, 16
    ret

/* ------------------------------------------------------------
 * Update: read joystick 1, move actors, collect ingredients, collide.
 * ------------------------------------------------------------ */
you_have_got_pizza_update:
    addi    sp, sp, -32
    sw      ra, 28(sp)
    sw      s0, 24(sp)
    sw      s1, 20(sp)

    la      t0, frame_no
    lw      t1, 0(t0)
    addi    t1, t1, 1
    sw      t1, 0(t0)

    call    prg32_input_read
    mv      s0, a0                  /* keep input in a saved register */

    la      t0, game_over
    lw      t1, 0(t0)
    beqz    t1, .L_update_playing
    andi    t2, s0, BTN_START
    beqz    t2, .L_update_done
    call    you_have_got_pizza_init
    j       .L_update_done

.L_update_playing:
    la      t0, message_timer
    lw      t1, 0(t0)
    beqz    t1, 1f
    addi    t1, t1, -1
    sw      t1, 0(t0)
1:
    /* Horizontal movement. */
    la      t0, player_x
    lw      t1, 0(t0)
    andi    t2, s0, BTN_LEFT
    beqz    t2, 2f
    addi    t1, t1, -3
2:  andi    t2, s0, BTN_RIGHT
    beqz    t2, 3f
    addi    t1, t1, 3
3:  li      t2, 8
    bge     t1, t2, 4f
    mv      t1, t2
4:  li      t2, 302
    ble     t1, t2, 5f
    mv      t1, t2
5:  sw      t1, 0(t0)

    /* Vertical movement is only legal when the player is close to a ladder.
       Keep the row in s1 while calling the audio routine, because t registers
       are caller-saved by the RISC-V ABI. */
    call    player_on_ladder
    beqz    a0, .L_no_ladder_move
    la      t0, player_row
    lw      s1, 0(t0)
    andi    t2, s0, BTN_UP
    beqz    t2, 6f
    beqz    s1, 6f
    addi    s1, s1, -1
    li      a0, 330
    li      a1, 18
    call    pizza_audio_note
6:  andi    t2, s0, BTN_DOWN
    beqz    t2, 7f
    li      t3, 3
    bge     s1, t3, 7f
    addi    s1, s1, 1
    li      a0, 220
    li      a1, 18
    call    pizza_audio_note
7:  la      t0, player_row
    sw      s1, 0(t0)
.L_no_ladder_move:
    la      t0, player_row
    lw      a0, 0(t0)
    call    row_to_y
    la      t0, player_y
    sw      a0, 0(t0)

    call    collect_asm
    call    move_enemies_asm
    call    collide_asm

.L_update_done:
    lw      s1, 20(sp)
    lw      s0, 24(sp)
    lw      ra, 28(sp)
    addi    sp, sp, 32
    ret

/* Helper: collect_asm. */
collect_asm:
    addi    sp, sp, -32
    sw      ra, 28(sp)
    sw      s0, 24(sp)
    sw      s1, 20(sp)

    li      s0, 0                  /* s0 is the ingredient index. */
.L_collect_loop:
    li      t5, NUM_ING
    bge     s0, t5, .L_check_complete

    la      t1, ing_collected
    add     t1, t1, s0
    lbu     t2, 0(t1)
    bnez    t2, .L_collect_next

    la      t2, ing_row
    add     t2, t2, s0
    lbu     t2, 0(t2)
    la      t3, player_row
    lw      t3, 0(t3)
    bne     t2, t3, .L_collect_next

    la      t2, ing_x
    slli    t3, s0, 2
    add     t2, t2, t3
    lw      t2, 0(t2)
    la      t3, player_x
    lw      t3, 0(t3)
    addi    t3, t3, 5
    sub     t4, t3, t2
    bge     t4, zero, .L_abs_done
    neg     t4, t4
.L_abs_done:
    li      t5, 14
    bge     t4, t5, .L_collect_next

    li      t2, 1
    la      t1, ing_collected
    add     t1, t1, s0
    sb      t2, 0(t1)
    la      t1, score
    lw      t2, 0(t1)
    addi    t2, t2, 25
    sw      t2, 0(t1)
    li      a0, 660
    li      a1, 45
    call    pizza_audio_note

.L_collect_next:
    addi    s0, s0, 1
    j       .L_collect_loop

.L_check_complete:
    li      s0, 0
.L_complete_loop:
    li      t5, NUM_ING
    bge     s0, t5, .L_complete_yes
    la      t1, ing_collected
    add     t1, t1, s0
    lbu     t2, 0(t1)
    beqz    t2, .L_collect_return
    addi    s0, s0, 1
    j       .L_complete_loop

.L_complete_yes:
    la      t1, score
    lw      t2, 0(t1)
    addi    t2, t2, 250
    sw      t2, 0(t1)
    la      t1, fed_students
    lw      t2, 0(t1)
    addi    t2, t2, 1
    sw      t2, 0(t1)
    li      t2, 100
    la      t1, message_timer
    sw      t2, 0(t1)
    call    setup_ingredients
    call    reset_enemies
    li      a0, 988
    li      a1, 100
    call    pizza_audio_note

.L_collect_return:
    lw      s1, 20(sp)
    lw      s0, 24(sp)
    lw      ra, 28(sp)
    addi    sp, sp, 32
    ret

/* Helper: move_enemies_asm. */
move_enemies_asm:
    li      t0, 0
1:  li      t6, NUM_ENEMY
    bge     t0, t6, 5f
    la      t1, enemy_x
    slli    t2, t0, 2
    add     t1, t1, t2
    lw      t3, 0(t1)
    la      t4, enemy_dir
    add     t4, t4, t0
    lb      t5, 0(t4)
    add     t3, t3, t5
    li      t6, 10
    bge     t3, t6, 2f
    li      t3, 10
    li      t5, 1
    sb      t5, 0(t4)
2:  li      t6, 300
    ble     t3, t6, 3f
    li      t3, 300
    li      t5, -1
    sb      t5, 0(t4)
3:  sw      t3, 0(t1)
    /* Occasionally let enemies climb or descend based on frame number. */
    la      t1, frame_no
    lw      t2, 0(t1)
    add     t2, t2, t0
    andi    t2, t2, 0x7f
    bnez    t2, 4f
    la      t1, enemy_row
    add     t1, t1, t0
    lbu     t2, 0(t1)
    andi    t3, t0, 1
    beqz    t3, 31f
    li      t4, 3
    bge     t2, t4, 4f
    addi    t2, t2, 1
    sb      t2, 0(t1)
    j       4f
31: beqz    t2, 4f
    addi    t2, t2, -1
    sb      t2, 0(t1)
4:  addi    t0, t0, 1
    j       1b
5:  ret

/* Helper: collide_asm. */
collide_asm:
    addi    sp, sp, -16
    sw      ra, 12(sp)
    li      t0, 0
1:  li      t6, NUM_ENEMY
    bge     t0, t6, 4f
    la      t1, enemy_row
    add     t1, t1, t0
    lbu     t2, 0(t1)
    la      t3, player_row
    lw      t3, 0(t3)
    bne     t2, t3, 3f
    la      t1, enemy_x
    slli    t2, t0, 2
    add     t1, t1, t2
    lw      t2, 0(t1)
    addi    t2, t2, 5
    la      t3, player_x
    lw      t3, 0(t3)
    addi    t3, t3, 5
    sub     t4, t2, t3
    bge     t4, zero, 2f
    neg     t4, t4
2:  li      t5, 12
    bge     t4, t5, 3f
    la      t1, lives
    lw      t2, 0(t1)
    addi    t2, t2, -1
    sw      t2, 0(t1)
    li      a0, 120
    li      a1, 150
    call    pizza_audio_note
    la      t1, lives
    lw      t2, 0(t1)
    blez    t2, 5f
    call    reset_player
    call    reset_enemies
    j       4f
3:  addi    t0, t0, 1
    j       1b
4:  lw      ra, 12(sp)
    addi    sp, sp, 16
    ret
5:  li      t2, 1
    la      t1, game_over
    sw      t2, 0(t1)
    li      t2, 255
    la      t1, message_timer
    sw      t2, 0(t1)
    j       4b

/* ------------------------------------------------------------
 * Drawing helpers.  These are intentionally simple: rectangles, text, and loops.
 * ------------------------------------------------------------ */
draw_piazza_asm:
    addi    sp, sp, -16
    sw      ra, 12(sp)
    li      a0, SKY
    call    prg32_gfx_clear

    /* University skyline. */
    li      a0, 0
    li      a1, 24
    li      a2, 320
    li      a3, 26
    li      a4, 0x632c
    call    prg32_gfx_rect

    /* Stone platforms. */
    li      s0, 0
1:  li      t0, 4
    bge     s0, t0, 2f
    la      t1, row_y
    slli    t2, s0, 1
    add     t1, t1, t2
    lhu     s1, 0(t1)
    li      a0, 8
    mv      a1, s1
    li      a2, 304
    li      a3, 5
    li      a4, STONE_DK
    call    prg32_gfx_rect
    li      a0, 8
    addi    a1, s1, 5
    li      a2, 304
    li      a3, 5
    li      a4, STONE
    call    prg32_gfx_rect
    addi    s0, s0, 1
    j       1b
2:
    /* Ladders. */
    li      s0, 0
3:  li      t0, 4
    bge     s0, t0, 4f
    la      t1, ladder_x
    slli    t2, s0, 1
    add     t1, t1, t2
    lhu     s1, 0(t1)
    addi    a0, s1, -6
    li      a1, 48
    li      a2, 3
    li      a3, 130
    li      a4, STONE_DK
    call    prg32_gfx_rect
    addi    a0, s1, 5
    li      a1, 48
    li      a2, 3
    li      a3, 130
    li      a4, STONE_DK
    call    prg32_gfx_rect
    addi    s0, s0, 1
    j       3b
4:
    lw      ra, 12(sp)
    addi    sp, sp, 16
    ret

draw_ingredients_asm:
    addi    sp, sp, -16
    sw      ra, 12(sp)
    li      s0, 0
1:  li      t0, NUM_ING
    bge     s0, t0, 3f
    la      t1, ing_x
    slli    t2, s0, 2
    add     t1, t1, t2
    lw      s1, 0(t1)
    la      t1, ing_row
    add     t1, t1, s0
    lbu     t2, 0(t1)
    la      t3, row_y
    slli    t2, t2, 1
    add     t3, t3, t2
    lhu     s2, 0(t3)
    addi    s2, s2, -12
    la      t1, ing_collected
    add     t1, t1, s0
    lbu     t2, 0(t1)
    bnez    t2, 2f
    la      t1, ing_kind
    add     t1, t1, s0
    lbu     a0, 0(t1)
    call    color_from_kind
    mv      a4, a0
    j       21f
2:  li      a4, STONE
21: addi    a0, s1, -13
    mv      a1, s2
    li      a2, 26
    li      a3, 8
    call    prg32_gfx_rect
    addi    s0, s0, 1
    j       1b
3:  lw      ra, 12(sp)
    addi    sp, sp, 16
    ret

draw_player_asm:
    addi    sp, sp, -32
    sw      ra, 28(sp)
    sw      s0, 24(sp)
    sw      s1, 20(sp)
    sw      s2, 16(sp)
    la      t0, player_x
    lw      s0, 0(t0)
    la      t0, player_y
    lw      s1, 0(t0)

    /*
     * Four-frame animated professor sprite.
     * Teaching point: an animation frame is just a few bits extracted from a
     * counter.  Here frame_no >> 3 slows the animation down to a readable pace.
     */
    la      t0, frame_no
    lw      t0, 0(t0)
    srli    t0, t0, 3
    andi    s2, t0, 3

    addi    a0, s0, 2
    mv      a1, s1
    li      a2, 6
    li      a3, 4
    li      a4, 0xfbe0
    call    prg32_gfx_rect
    addi    a0, s0, 1
    addi    a1, s1, 4
    li      a2, 8
    li      a3, 7
    li      a4, BLUE
    call    prg32_gfx_rect
    addi    a0, s0, 4
    addi    a1, s1, 5
    li      a2, 2
    li      a3, 6
    li      a4, WHITE
    call    prg32_gfx_rect

    /* Alternate arms. */
    andi    t1, s2, 1
    bnez    t1, 1f
    mv      a0, s0
    addi    a1, s1, 6
    li      a2, 2
    li      a3, 4
    li      a4, 0xfbe0
    call    prg32_gfx_rect
    addi    a0, s0, 8
    addi    a1, s1, 7
    li      a2, 2
    li      a3, 4
    li      a4, 0xfbe0
    call    prg32_gfx_rect
    j       2f
1:  mv      a0, s0
    addi    a1, s1, 7
    li      a2, 2
    li      a3, 4
    li      a4, 0xfbe0
    call    prg32_gfx_rect
    addi    a0, s0, 8
    addi    a1, s1, 6
    li      a2, 2
    li      a3, 4
    li      a4, 0xfbe0
    call    prg32_gfx_rect
2:
    /* Alternate legs. */
    andi    t1, s2, 1
    bnez    t1, 3f
    addi    a0, s0, 1
    addi    a1, s1, 11
    li      a2, 3
    li      a3, 3
    li      a4, SHADOW
    call    prg32_gfx_rect
    addi    a0, s0, 6
    addi    a1, s1, 11
    li      a2, 3
    li      a3, 2
    li      a4, SHADOW
    call    prg32_gfx_rect
    j       4f
3:  addi    a0, s0, 1
    addi    a1, s1, 11
    li      a2, 3
    li      a3, 2
    li      a4, SHADOW
    call    prg32_gfx_rect
    addi    a0, s0, 6
    addi    a1, s1, 11
    li      a2, 3
    li      a3, 3
    li      a4, SHADOW
    call    prg32_gfx_rect
4:  lw      s2, 16(sp)
    lw      s1, 20(sp)
    lw      s0, 24(sp)
    lw      ra, 28(sp)
    addi    sp, sp, 32
    ret

draw_enemies_asm:
    addi    sp, sp, -32
    sw      ra, 28(sp)
    sw      s0, 24(sp)
    sw      s1, 20(sp)
    sw      s2, 16(sp)
    sw      s3, 12(sp)
    li      s0, 0
1:  li      t0, NUM_ENEMY
    bge     s0, t0, 2f
    la      t1, enemy_x
    slli    t2, s0, 2
    add     t1, t1, t2
    lw      s1, 0(t1)
    la      t1, enemy_row
    add     t1, t1, s0
    lbu     t2, 0(t1)
    la      t3, row_y
    slli    t2, t2, 1
    add     t3, t3, t2
    lhu     s2, 0(t3)
    addi    s2, s2, -ENEMY_H

    /* Student animation: frame = ((frame_no >> 3) + index) & 3. */
    la      t0, frame_no
    lw      t0, 0(t0)
    srli    t0, t0, 3
    add     t0, t0, s0
    andi    s3, t0, 1

    addi    a0, s1, 2
    mv      a1, s2
    li      a2, 6
    li      a3, 4
    li      a4, 0xffbe
    call    prg32_gfx_rect
    mv      a0, s1
    addi    a1, s2, 4
    li      a2, 10
    li      a3, 6
    li      a4, MAGENTA
    call    prg32_gfx_rect

    bnez    s3, 11f
    mv      a0, s1
    addi    a1, s2, 5
    li      a2, 2
    li      a3, 2
    li      a4, 0xffbe
    call    prg32_gfx_rect
    addi    a0, s1, 7
    addi    a1, s2, 10
    li      a2, 3
    li      a3, 4
    li      a4, SHADOW
    call    prg32_gfx_rect
    j       12f
11: mv      a0, s1
    addi    a1, s2, 7
    li      a2, 2
    li      a3, 2
    li      a4, 0xffbe
    call    prg32_gfx_rect
    addi    a0, s1, 2
    addi    a1, s2, 10
    li      a2, 3
    li      a3, 4
    li      a4, SHADOW
    call    prg32_gfx_rect
12: addi    s0, s0, 1
    j       1b
2:  lw      s3, 12(sp)
    lw      s2, 16(sp)
    lw      s1, 20(sp)
    lw      s0, 24(sp)
    lw      ra, 28(sp)
    addi    sp, sp, 32
    ret

draw_hud_asm:
    addi    sp, sp, -16
    sw      ra, 12(sp)
    li      a0, 0
    li      a1, 0
    li      a2, 320
    li      a3, 16
    li      a4, BLACK
    call    prg32_gfx_rect
    li      a0, 4
    li      a1, 4
    la      a2, title_txt
    li      a3, YELLOW
    li      a4, BLACK
    call    prg32_gfx_text8
    li      a0, 194
    li      a1, 4
    la      a2, score_txt
    li      a3, WHITE
    li      a4, BLACK
    call    prg32_gfx_text8
    li      a0, 260
    li      a1, 4
    la      a2, lives_txt
    li      a3, WHITE
    li      a4, BLACK
    call    prg32_gfx_text8
    /* Draw one yellow square per remaining life. */
    la      t0, lives
    lw      s0, 0(t0)
    li      s1, 0
1:  bge     s1, s0, 2f
    slli    t1, s1, 3
    addi    a0, t1, 304
    li      a1, 5
    li      a2, 5
    li      a3, 5
    li      a4, YELLOW
    call    prg32_gfx_rect
    addi    s1, s1, 1
    j       1b
2:  lw      ra, 12(sp)
    addi    sp, sp, 16
    ret

/* ------------------------------------------------------------
 * Draw: never mutates gameplay. It only visualizes the current model.
 * ------------------------------------------------------------ */
you_have_got_pizza_draw:
    addi    sp, sp, -32
    sw      ra, 28(sp)
    sw      s0, 24(sp)
    sw      s1, 20(sp)
    sw      s2, 16(sp)
    call    draw_piazza_asm
    call    draw_ingredients_asm
    call    draw_enemies_asm
    call    draw_player_asm
    call    draw_hud_asm

    li      a0, 8
    li      a1, 186
    la      a2, hint_txt
    li      a3, YELLOW
    li      a4, BLACK
    call    prg32_gfx_text8

    la      t0, message_timer
    lw      t1, 0(t0)
    beqz    t1, .L_draw_done
    li      a0, 52
    li      a1, 68
    li      a2, 216
    li      a3, 40
    li      a4, BLACK
    call    prg32_gfx_rect
    la      t0, game_over
    lw      t1, 0(t0)
    beqz    t1, 1f
    li      a0, 76
    li      a1, 84
    la      a2, over_txt
    li      a3, RED
    li      a4, BLACK
    call    prg32_gfx_text8
    j       .L_draw_done
1:  la      t0, fed_students
    lw      t1, 0(t0)
    beqz    t1, 2f
    li      a0, 88
    li      a1, 84
    la      a2, ready_txt
    li      a3, YELLOW
    li      a4, BLACK
    call    prg32_gfx_text8
    j       .L_draw_done
2:  li      a0, 64
    li      a1, 84
    la      a2, start_txt
    li      a3, WHITE
    li      a4, BLACK
    call    prg32_gfx_text8

.L_draw_done:
    lw      s2, 16(sp)
    lw      s1, 20(sp)
    lw      s0, 24(sp)
    lw      ra, 28(sp)
    addi    sp, sp, 32
    ret
