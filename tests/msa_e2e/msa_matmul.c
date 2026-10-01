// End-to-end test of the multi-precision array (msa_vpush / msa_vpop): descriptor ->
// MVIN -> packed 32-bit words pushed -> pop -> MVOUT, checked against a reference
// computed here. Values are exact in every format, so the check is bit-exact.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define SPAD_VADDR 0xD0000000UL
#define W_OFF 0
#define X_OFF 4096
#define Y_OFF 8192
#define MAXL 32
#define R 4

typedef struct {
  uint32_t dim_size[4];      // +0
  uint32_t dim_low[4];       // +16
  uint32_t dim_high[4];      // +32
  uint64_t mm_stride[4];     // +48
  uint64_t spad_stride[4];   // +80
  uint16_t element_size;     // +112
  uint16_t vlane_stride;     // +114
  uint8_t  vlane_split_axis; // +116
  uint8_t  dtype;            // +117
  uint16_t flags;            // +118
  uint64_t indirect_addr;    // +120
  uint16_t indirect_stride;  // +128
  uint16_t indirect_esize;   // +130
  uint8_t  indirect_dim;     // +132
  uint8_t  pad132;
  uint16_t indirect_lanes;   // +134
  uint64_t fill;             // +136
  uint64_t dram_base;        // +144
  uint64_t dram_bytes;       // +152
} desc_t;

#define CONFIG_DESC(p) \
  asm volatile(".insn r 0x2b, 3, 7, x0, %0, x0" :: "r"(p) : "memory")
#define MVIN(dram, spad) \
  asm volatile(".insn r 0x2b, 3, 2, x0, %0, %1" :: "r"(dram), "r"(spad) : "memory")
#define MVOUT(dram, spad) \
  asm volatile(".insn r 0x2b, 3, 3, x0, %0, %1" :: "r"(dram), "r"(spad) : "memory")

// msa_vpush = sf.vc.iv opcode 2 (0x2A00305B), msa_vpop = sf.vc.v.i opcode 3
// (0x0C00305B). SIMM5 rides bits 19:15: [4] weight, [3:2] width shift, [1:0] format.
#define XS(x) #x
#define S(x) XS(x)
#define M_VPUSH(vs, s5) ".word " S(0x2A00305B + ((vs) << 20) + ((s5) << 15)) "\n\t"
#define M_VPOP(vd, s5)  ".word " S(0x0C00305B + ((vd) << 7) + ((s5) << 15)) "\n\t"

enum { F32 = 0, F16 = 1, E4M3 = 2, E5M2 = 3 };
static const char *fmt_name[] = {"f32", "f16", "e4m3", "e5m2"};

static desc_t g_dw, g_dx, g_dy;
static uint32_t g_W[MAXL * MAXL], g_X[MAXL * R];
static float g_Y[R * MAXL];
static float g_A[R][MAXL * 4], g_B[MAXL * 4][MAXL];

#define BODY(WS5, IS5)                                                     \
  asm volatile(                                                            \
    "vsetvli %0, %1, e32, m4, ta, ma\n\t"                                  \
    "vle32.v v8, (%2)\n\t"                                                 \
    M_VPUSH(8, WS5)                                                        \
    "vsetvli %0, %3, e32, m4, ta, ma\n\t"                                  \
    "vle32.v v12, (%4)\n\t"                                                \
    M_VPUSH(12, IS5)                                                       \
    M_VPOP(16, 0)                                                          \
    "vse32.v v16, (%5)\n\t"                                                \
    : "=&r"(t)                                                             \
    : "r"(nvu), "r"(ws), "r"(rr), "r"(xs), "r"(ys)                         \
    : "memory")

#define CASE(F, SH) case (F) * 4 + (SH): BODY(16 + (SH) * 4 + (F), (SH) * 4 + (F)); break;

__attribute__((noinline, aligned(64)))
void npu_kernel(long nvu, int fmt, int shift)
{
  unsigned long ws = SPAD_VADDR + W_OFF, xs = SPAD_VADDR + X_OFF, ys = SPAD_VADDR + Y_OFF;
  long rr = R, t;
  CONFIG_DESC(&g_dw); MVIN(g_W, ws);
  CONFIG_DESC(&g_dx); MVIN(g_X, xs);
  switch (fmt * 4 + shift) {
    CASE(0, 0) CASE(0, 2) CASE(1, 0) CASE(1, 2)   /* f32, f16 */
    CASE(2, 0) CASE(2, 2) CASE(3, 0) CASE(3, 2)   /* e4m3, e5m2 */
  }
  CONFIG_DESC(&g_dy); MVOUT(g_Y, ys);
}

static void fill_desc(desc_t *d, int lane_dim, int elt_dim, uint64_t mm_lane,
                      uint64_t mm_elt, uint64_t spad_lane)
{
  memset(d, 0, sizeof(*d));
  d->dim_size[0] = 1; d->dim_size[1] = 1;
  d->dim_size[2] = lane_dim; d->dim_size[3] = elt_dim;
  d->mm_stride[2] = mm_lane; d->mm_stride[3] = mm_elt;
  d->spad_stride[2] = spad_lane; d->spad_stride[3] = 1;
  d->element_size = 4;
  d->vlane_stride = 1;
  d->vlane_split_axis = 2;
}

// Bits of v in the format; v is 0 or +-{0.5,1,1.5,2}, exact in all four.
static uint32_t encode(float v, int fmt)
{
  uint32_t s = v < 0, b;
  float a = v < 0 ? -v : v;
  int e = 0;
  if (fmt == F32) { memcpy(&b, &v, 4); return b; }
  if (a == 0.0f) return 0;
  while (a >= 2.0f) { a /= 2; e++; }
  while (a < 1.0f) { a *= 2; e--; }
  float m = a - 1.0f;
  if (fmt == F16)  return (s << 15) | ((uint32_t)(e + 15) << 10) | (uint32_t)(m * 1024);
  if (fmt == E4M3) return (s << 7) | ((uint32_t)(e + 7) << 3) | (uint32_t)(m * 8);
  return (s << 7) | ((uint32_t)(e + 15) << 2) | (uint32_t)(m * 4);
}

static const float vals[] = {-2, -1.5, -1, -0.5, 0, 0.5, 1, 1.5, 2};

static int run_case(int nvu, int fmt, int shift)
{
  int pk = fmt == F32 ? 1 : fmt == F16 ? 2 : 4, eb = 32 / pk;
  int K = nvu * pk, active = nvu >> shift, bad = 0;

  for (int m = 0; m < R; m++)
    for (int k = 0; k < K; k++) g_A[m][k] = vals[(m * 7 + k * 3 + fmt) % 9];
  for (int k = 0; k < K; k++)
    for (int n = 0; n < nvu; n++) g_B[k][n] = vals[(k * 5 + n * 2 + shift) % 9];

  // W: lane n, word i = B[i*pk .. i*pk+pk)[n].  X: lane j, word m = A[m][j*pk ..].
  for (int n = 0; n < nvu; n++)
    for (int i = 0; i < nvu; i++) {
      uint32_t w = 0;
      for (int s = 0; s < pk; s++) w |= encode(g_B[i * pk + s][n], fmt) << (s * eb);
      g_W[n * nvu + i] = w;
    }
  for (int j = 0; j < nvu; j++)
    for (int m = 0; m < R; m++) {
      uint32_t w = 0;
      for (int s = 0; s < pk; s++) w |= encode(g_A[m][j * pk + s], fmt) << (s * eb);
      g_X[j * R + m] = w;
    }
  fill_desc(&g_dw, nvu, nvu, nvu, 1, nvu);
  fill_desc(&g_dx, nvu, R, R, 1, R);
  fill_desc(&g_dy, nvu, R, 1, nvu, R);
  memset(g_Y, 0xA5, sizeof(g_Y));

  npu_kernel(nvu, fmt, shift);

  for (int m = 0; m < R; m++)
    for (int n = 0; n < nvu; n++) {
      float want = 0;
      if (n < active)
        for (int k = 0; k < K; k++) want += g_A[m][k] * g_B[k][n];
      float got = g_Y[m * nvu + n];
      if (got != want) {
        if (bad < 6) printf("  [m=%d n=%d] got %g want %g\n", m, n, got, want);
        bad++;
      }
    }
  printf("%-4s fmt=%-4s pack=%d K=%-3d width=%d/%d lanes=%d  %d/%d ok\n",
         bad ? "FAIL" : "PASS", fmt_name[fmt], pk, K, active, nvu, nvu,
         R * nvu - bad, R * nvu);
  return bad;
}

int main(int argc, char **argv)
{
  int nvu = argc > 1 ? atoi(argv[1]) : 32, bad = 0;
  if (nvu > MAXL) { printf("lanes > %d\n", MAXL); return 1; }
  printf("msa e2e, lanes=%d\n", nvu);
  for (int fmt = 0; fmt < 4; fmt++)
    for (int shift = 0; shift <= 2; shift += 2)
      bad += run_case(nvu, fmt, shift);
  printf("%s: %d mismatching element(s)\n", bad ? "FAILED" : "ALL PASS", bad);
  return bad != 0;
}
