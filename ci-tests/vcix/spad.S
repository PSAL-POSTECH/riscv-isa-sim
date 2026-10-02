# Writes and reads back the first doubleword of the scratchpad and the one that
# ends OFFSET bytes past its end: OFFSET 0 is the last doubleword inside.
# SPAD_BASE is the base, SPAD_SIZE the size of one lane times the lanes.
#ifndef OFFSET
#define OFFSET 0
#endif
    .text
    .globl main
main:
    li      t0, SPAD_BASE
    li      t1, 0x1122334455667788
    sd      t1, 0(t0)
    ld      t2, 0(t0)
    bne     t1, t2, fail
    li      t3, SPAD_SIZE - 8 + OFFSET
    add     t3, t0, t3
    not     t1, t1
    sd      t1, 0(t3)
    ld      t2, 0(t3)
    bne     t1, t2, fail
    not     t1, t1
    ld      t2, 0(t0)
    bne     t1, t2, fail
    li      a0, 0
    ret
fail:
    li      a0, 1
    ret
