// Zvfofp8min target: runs vfncvt(.sat).f.f.q, vfncvtbf16(.sat).f.f.w and vfwcvtbf16.f.f.v at SEW=8
// for both formats (altfmt) under every frm, one element at a time (exact fflags) and in vector chunks,
// writing 16-byte records {op,mode,flags,vec,chunk,in,out} to stdout. Instructions are .word (binutils 2.41).
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define VFNCVT_Q    ".word 0x4a8c9257\n"   /* vfncvt.f.f.q       v4, v8  */
#define VFNCVT_QS   ".word 0x4a8d9257\n"   /* vfncvt.sat.f.f.q   v4, v8  */
#define VFNCVTBF    ".word 0x4a8e9257\n"   /* vfncvtbf16.f.f.w     v4, v8 */
#define VFNCVTBF_S  ".word 0x4a8f9257\n"   /* vfncvtbf16.sat.f.f.w v4, v8 */
#define VFWCVTBF    ".word 0x4a869857\n"   /* vfwcvtbf16.f.f.v    v16, v8 */

typedef struct { uint8_t op, mode, flags, vec; uint32_t chunk, in, out; } rec_t;
static rec_t buf[4096]; static unsigned nbuf; static uint32_t chunk_id;
static void emit(uint8_t op, uint8_t mode, uint8_t fl, uint8_t vec, uint32_t in, uint32_t out) {
  rec_t r = {op, mode, fl, vec, chunk_id, in, out};
  buf[nbuf++] = r;
  if (nbuf == 4096) { fwrite(buf, sizeof(rec_t), nbuf, stdout); nbuf = 0; }
}
static uint64_t rs = 0x9E3779B97F4A7C15ull;
static uint32_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return (uint32_t)(rs >> 16); }

#define VT(fmt) ((unsigned long)(0xC1 | ((fmt) << 8)))   /* e8, m2, ta, ma, altfmt=fmt */
static uint32_t f32in[8192]; static unsigned nf32;
static uint16_t bfin[65536];
static uint8_t o8[64]; static uint16_t o16[64];

#define NARROW(INSN, LOAD, src)                                                   \
  asm volatile("csrw frm, %1\n csrw fflags, x0\n vsetvl t0, %2, %3\n" LOAD " v8, (%4)\n" \
               INSN " vse8.v v4, (%5)\n csrr %0, fflags"                          \
               : "=r"(fl) : "r"(frm), "r"(n), "r"(VT(fmt)), "r"(src), "r"(o8) : "t0", "memory")

static unsigned long q(int sat, unsigned fmt, unsigned frm, const uint32_t *src, unsigned long n) {
  unsigned long fl;
  if (sat) NARROW(VFNCVT_QS, "vle32.v", src); else NARROW(VFNCVT_Q, "vle32.v", src);
  return fl;
}
static unsigned long nb(int sat, unsigned fmt, unsigned frm, const uint16_t *src, unsigned long n) {
  unsigned long fl;
  if (sat) NARROW(VFNCVTBF_S, "vle16.v", src); else NARROW(VFNCVTBF, "vle16.v", src);
  return fl;
}
static unsigned long w(unsigned fmt, unsigned frm, const uint8_t *src, unsigned long n) {
  unsigned long fl;
  asm volatile("csrw frm, %1\n csrw fflags, x0\n vsetvl t0, %2, %3\n vle8.v v8, (%4)\n"
               VFWCVTBF " vse16.v v16, (%5)\n csrr %0, fflags"
               : "=r"(fl) : "r"(frm), "r"(n), "r"(VT(fmt)), "r"(src), "r"(o16) : "t0", "memory");
  return fl;
}
static unsigned long vlmax(unsigned fmt) {
  unsigned long vl; asm volatile("vsetvl %0, %1, %2" : "=r"(vl) : "r"(-1L), "r"(VT(fmt))); return vl;
}
static uint32_t fbits(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }
static void addf(uint32_t u) { f32in[nf32++] = u; f32in[nf32++] = u ^ 0x80000000u; }

static void build_f32(void) {
  static const float c[] = {0.0f, 1.0f, 448.0f, 449.0f, 456.0f, 460.0f, 464.0f, 468.0f, 472.0f, 476.0f, 480.0f,
    496.0f, 512.0f, 1e30f, 3.4028235e38f, 57344.0f, 57345.0f, 59392.0f, 60000.0f, 61440.0f, 63000.0f, 65536.0f,
    0x1p-6f, 0x1p-9f, 0x1p-10f, 0x1.8p-10f, 0x1.8p-9f, 0x1.cp-7f, 0x1.ep-7f, 0x1.fp-7f, 0x1p-14f, 0x1p-16f,
    0x1p-17f, 0x1.8p-17f, 0x1.8p-16f, 0x1.cp-15f, 0x1.ep-15f, 0x1p-30f, 1.0625f, 1.1875f, 1.125f, 1.375f,
    1.0624999f, 1.0625001f, 1.12499f, 1.12501f, 3.14159265f, 0.1f};
  for (unsigned i = 0; i < sizeof c / sizeof c[0]; i++) {
    uint32_t u = fbits(c[i]); addf(u);
    if (u && (u & 0x7f800000u) != 0x7f800000u) { addf(u + 1); addf(u - 1); }
  }
  uint32_t sp[] = {0x7f800000u, 0x7fc00000u, 0x7f800001u, 0x7fa00000u, 0x00000001u, 0x007fffffu, 0x00400000u};
  for (unsigned i = 0; i < sizeof sp / 4; i++) addf(sp[i]);
  for (unsigned k = 0; k < 32; k++) { addf(fbits((float)(2 * k + 1) * 0x1p-10f)); addf(fbits((float)(2 * k + 1) * 0x1p-17f)); }
  while (nf32 < 8192) {
    uint32_t r = rnd(), e = 100 + rnd() % 46, m = rnd() & 0x7fffff;
    switch (r % 5) {
      case 1: m = (m & ~0xfffffu) | 0x80000u; break;
      case 2: m = (m & ~0x1fffffu) | 0x100000u; break;
      case 3: m = (m & ~0xfffffu) | 0x80000u; m += (r & 8) ? 1 : -1; break;
      case 4: e = rnd() % 256; break;
    }
    f32in[nf32++] = (r & 0x80000000u) | (e << 23) | (m & 0x7fffff);
  }
}

int main(void) {
  unsigned long vlenb; asm volatile("csrr %0, vlenb" : "=r"(vlenb));
  chunk_id = vlenb; emit(255, 0, 0, 0, 0, 0); chunk_id = 0;
  build_f32();
  for (unsigned i = 0; i < 65536; i++) bfin[i] = i;
  static uint8_t f8in[256]; for (unsigned i = 0; i < 256; i++) f8in[i] = i;
  for (unsigned fmt = 0; fmt < 2; fmt++)
    for (unsigned frm = 0; frm < 5; frm++) {
      unsigned long vm = vlmax(fmt), n, fl;
      for (int sat = 0; sat < 2; sat++) {
        uint8_t mode = fmt | sat << 1 | frm << 2;
        for (unsigned i = 0; i < nf32; i++) { fl = q(sat, fmt, frm, &f32in[i], 1); emit(sat, mode, fl, 0, f32in[i], o8[0]); }
        for (unsigned i = 0; i < nf32; i += n, chunk_id++) {
          n = nf32 - i < vm ? nf32 - i : vm; fl = q(sat, fmt, frm, &f32in[i], n);
          for (unsigned j = 0; j < n; j++) emit(sat, mode, fl, 1, f32in[i + j], o8[j]);
        }
        for (unsigned i = 0; i < 65536; i++) { fl = nb(sat, fmt, frm, &bfin[i], 1); emit(2 + sat, mode, fl, 0, bfin[i], o8[0]); }
        for (unsigned i = 0; i < 65536; i += n, chunk_id++) {
          n = 65536 - i < vm ? 65536 - i : vm; fl = nb(sat, fmt, frm, &bfin[i], n);
          for (unsigned j = 0; j < n; j++) emit(2 + sat, mode, fl, 1, bfin[i + j], o8[j]);
        }
      }
      uint8_t mode = fmt | frm << 2;
      for (unsigned i = 0; i < 256; i++) { fl = w(fmt, frm, &f8in[i], 1); emit(4, mode, fl, 0, f8in[i], o16[0]); }
      for (unsigned i = 0; i < 256; i += n, chunk_id++) {
        n = 256 - i < vm ? 256 - i : vm; fl = w(fmt, frm, &f8in[i], n);
        for (unsigned j = 0; j < n; j++) emit(4, mode, fl, 1, f8in[i + j], o16[j]);
      }
    }
  fwrite(buf, sizeof(rec_t), nbuf, stdout);
  return 0;
}
