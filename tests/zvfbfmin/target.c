// Zvfbfmin target: vfncvtbf16.f.f.w under every frm and vfwcvtbf16.f.f.v, printing values and fflags.
// The two instructions are emitted as .word because binutils 2.41 does not know zvfbfmin.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
static uint32_t f32in[] = {0x3f800000,0x3f808000,0x3f818000,0x3f80c000,0x3f804000,0xbf808001,0x7f7fffff,0x7f800000,0xff800000,
  0x7fc00000,0x7f800001,0x00000001,0x00008000,0x00018000,0x80000000,0x00000000,0x3eaaaaab,0x477fe000,0x40490fdb,0xc2c80000};
#define N (sizeof(f32in)/4)
static uint16_t bfout[32]; static uint32_t f32out[32];
static uint16_t bfin[] = {0x3f80,0x7f80,0xff80,0x7fc0,0x7f81,0x0001,0x8001,0x0000,0x8000,0x4049,0xc2c8,0x7f7f};
#define M (sizeof(bfin)/2)
static unsigned long run_n(uint32_t frm){
  unsigned long vl, fl;
  asm volatile("csrw frm, %0; csrw fflags, x0" :: "r"(frm));
  asm volatile("vsetvli %0, %1, e16, m1, ta, ma" : "=r"(vl) : "r"(N));
  asm volatile("vsetvli x0, %0, e32, m2, ta, ma\n vle32.v v4, (%1)" :: "r"(N), "r"(f32in));
  asm volatile("vsetvli x0, %0, e16, m1, ta, ma\n .word 0x4a4e9457\n vse16.v v8, (%1)" :: "r"(N), "r"(bfout) : "memory");
  asm volatile("csrr %0, fflags" : "=r"(fl));
  return fl;
}
static unsigned long run_w(void){
  unsigned long fl;
  asm volatile("csrw fflags, x0");
  asm volatile("vsetvli x0, %0, e16, m1, ta, ma\n vle16.v v4, (%1)\n .word 0x4a469457" :: "r"(M), "r"(bfin));
  asm volatile("vsetvli x0, %0, e32, m2, ta, ma\n vse32.v v8, (%1)" :: "r"(M), "r"(f32out) : "memory");
  asm volatile("csrr %0, fflags" : "=r"(fl));
  return fl;
}
int main(){
  for (uint32_t frm=0; frm<5; frm++){ unsigned long fl=run_n(frm); printf("N frm=%u fflags=%02lx:", frm, fl); for (unsigned i=0;i<N;i++) printf(" %04x", bfout[i]); printf("\n"); }
  unsigned long fl=run_w(); printf("W fflags=%02lx:", fl); for (unsigned i=0;i<M;i++) printf(" %08x", f32out[i]); printf("\n");
  return 0;
}
