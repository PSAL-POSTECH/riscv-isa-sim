# One instruction in custom-2, sf.vc.x 0x1, 0x2, 0x3, a0. With VSET it is given a
# vtype first; without, vtype is still vill.
    .text
    .globl main
main:
#ifdef VSET
    li      t0, 8
    vsetvli t0, t0, e32, m1, ta, ma
#endif
    .word   0x062541db
    li      a0, 0
    ret
