# Returns VLEN in bytes.
    .text
    .globl main
main:
    csrr    a0, vlenb
    ret
