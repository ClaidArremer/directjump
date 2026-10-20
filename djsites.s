| SPDX-License-Identifier: GPL-2.0-or-later
| DIRECT JUMP (Digitone mk1, OS 1.44): the sequencer's two sites and the
| SETTINGS row. ColdFire V4; addresses from mod.json's defsym.
|
| The row's four callbacks follow the SETTINGS menu's MenuItem conventions as
| digihealth's SYSTEM INFO row does (irpina/digihealth, sysinfo.s,
| GPL-2.0-or-later), from which they are adapted, as PUMP's row is.

        .equ    SQ_LEN_OVR, DJ_SEQ + 0x2d8      | s16: the source's master length
        .equ    SQ_PENDING, DJ_SEQ + 0x276      | u16: tracks waiting for their step

        .section .run, "ax"

| ---- site A: 0x40095fdc, over `mvs.w 0x4138f2d8,d1` (6 bytes) ---------------
| The tick interrupt, two ticks into a master step, while playing, after the
| pattern source's next entry was peeked into 92(sp): d0 = its pattern, d3 =
| nonzero when song mode changes the source. dj_peek arms the change itself
| when a pattern was just picked; then the stock end-of-pattern test and its
| arm are skipped: back to where the stock arm goes on (DJ_ARMED_RET). Else
| back to the stock with d1 as the instruction left it. Keeps all registers
| but d1, and the stock reads no flags from it.
        .globl  dj_site_peek
dj_site_peek:
        mvs.w   SQ_LEN_OVR, %d1         | the instruction this replaced
        lea     -16(%sp), %sp
        movem.l %d0-%d1/%a0-%a1, (%sp)
        pea     112(%sp)                | the entry: 92 + 4 (return) + 16
        move.l  %d3, -(%sp)             | song
        move.l  %d0, -(%sp)             | next
        jsr     dj_peek
        lea     12(%sp), %sp
        tst.l   %d0
        movem.l (%sp), %d0-%d1/%a0-%a1  | movem and lea keep the flags
        lea     16(%sp), %sp
        beq.s   1f
        move.l  #DJ_ARMED_RET, (%sp)    | armed: on after the stock arm
1:      rts

| ---- site B: 0x4009643a, over `move.w d4,0x4138f276` (6 bytes) --------------
| The tick interrupt, on the switch path, after each track's step was worked
| out: d4 = the pending mask. dj_switched redoes the steps after a jump and
| gives the mask to store. d4 is not read after this (the code that follows
| loads it afresh).
        .globl  dj_site_switch
dj_site_switch:
        lea     -16(%sp), %sp
        movem.l %d0-%d1/%a0-%a1, (%sp)
        move.l  %d4, -(%sp)
        jsr     dj_switched
        addq.l  #4, %sp
        move.l  %d0, %d4
        movem.l (%sp), %d0-%d1/%a0-%a1
        lea     16(%sp), %sp
        move.w  %d4, SQ_PENDING         | the instruction this replaced
        rts

| ---- SETTINGS > DIRECT JUMP: a checkbox ---------------------------------------
| ev_settings handler: the row, after the stock ones (and other mods')
        .globl  dj_settings
dj_settings:
        pea     dj_row
        move.l  8(%sp), -(%sp)          | the menu
        jsr     core_additem
        addq.l  #8, %sp
        rts

| label() -> std::string, returned through a0
dj_label:
        link    %a6, #-4
        move.l  %d2, -(%sp)
        pea     -1(%a6)
        pea     dj_str
        move.l  %a0, %d2
        move.l  %a0, -(%sp)
        jsr     STR_CTOR
        lea     12(%sp), %sp
        move.l  %d2, %d0
        move.l  -8(%a6), %d2
        unlk    %a6
        rts

| select(payload, item): YES toggles it, then the menu redraws
dj_select:
        movea.l 4(%sp), %a0
        movea.l (%a0), %a0
        moveq   #0, %d0
        tst.b   dj_data
        seq     %d0
        andi.l  #1, %d0
        move.b  %d0, dj_data
        lea     0x38(%a0), %a0
        move.l  %a0, 4(%sp)
        jmp     INVALIDATE

| change(payload, item, delta): RIGHT ticks it, LEFT unticks it
dj_change:
        movea.l 4(%sp), %a0
        movea.l (%a0), %a0
        move.l  12(%sp), %d0
        beq.s   1f
        sgt     %d0
        andi.l  #1, %d0
        move.b  %d0, dj_data
        lea     0x38(%a0), %a0
        move.l  %a0, 4(%sp)
        jmp     INVALIDATE
1:      rts

| draw(payload, item, bmp, x, y): the checkbox, ticked while on
dj_draw:
        moveq   #0, %d0
        tst.b   dj_data
        beq.s   1f
        moveq   #0x1c, %d0
1:      add.l   CHECKBOXES, %d0
        move.l  12(%sp), %d1
        move.l  %d1, 4(%sp)
        move.l  %d0, 8(%sp)
        move.l  16(%sp), %d1
        move.l  %d1, 12(%sp)
        move.l  20(%sp), %d1
        move.l  %d1, 16(%sp)
        clr.l   20(%sp)
        jmp     BLIT

        .balign 4
dj_row: .long   dj_label, dj_select, dj_draw, dj_change
dj_str: .asciz  "DIRECT JUMP"
        .balign 4
