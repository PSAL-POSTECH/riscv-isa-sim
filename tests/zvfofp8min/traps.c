// Negative controls for tests/zvfofp8min: T selects one instruction (or a vtype probe) to run;
// a surviving instruction prints AFTER, a trap kills the program first. VTYPE probes print vtype.
#include <stdio.h>
int main(void) {
  static unsigned char b8[64]; static unsigned short b16[64]; static unsigned int b32[64];
  unsigned long vt = 0;
#if T == 0
  asm volatile("vsetivli t0, 4, e8, m1, ta, ma\n vle32.v v8, (%0)\n .word 0x4a8c9257" :: "r"(b32) : "t0");
#elif T == 1
  asm volatile("vsetivli t0, 4, e8, m1, ta, ma\n vle32.v v8, (%0)\n .word 0x4a8d9257" :: "r"(b32) : "t0");
#elif T == 2
  asm volatile("vsetivli t0, 4, e8, m1, ta, ma\n vle16.v v8, (%0)\n .word 0x4a8e9257" :: "r"(b16) : "t0");
#elif T == 3
  asm volatile("vsetivli t0, 4, e8, m1, ta, ma\n vle16.v v8, (%0)\n .word 0x4a8f9257" :: "r"(b16) : "t0");
#elif T == 4
  asm volatile("vsetivli t0, 4, e8, m1, ta, ma\n vle8.v v8, (%0)\n .word 0x4a869857" :: "r"(b8) : "t0");
#elif T == 5
  asm volatile("vsetivli t0, 4, e16, m1, ta, ma\n vle32.v v8, (%0)\n .word 0x4a8d9257" :: "r"(b32) : "t0");
#elif T == 6
  asm volatile("vsetivli t0, 4, e16, m1, ta, ma\n vle32.v v8, (%0)\n .word 0x4a8f9257" :: "r"(b32) : "t0");
#elif T == 7
  asm volatile("vsetivli t0, 4, e16, m1, ta, ma\n vle32.v v8, (%0)\n .word 0x4a8c9257" :: "r"(b32) : "t0");
#elif T >= 10
  asm volatile("vsetvl t0, %1, %2\n csrr %0, vtype" : "=r"(vt) : "r"(4L), "r"((unsigned long)VTYPE) : "t0");
#endif
  printf("AFTER vtype=%lx\n", vt);
  return 0;
}
