#ifndef _RISCV_MSA_UNIT_H
#define _RISCV_MSA_UNIT_H

// The multi-precision array: systolicArray_t's copy whose cell takes one 32-bit
// lane word per cycle, so one word is 1 f32, 2 f16 or 4 fp8 K-elements. The push's
// SIMM5 names the operand, the format and how many columns are live.

#include <cassert>
#include <cstdint>
#include <deque>
#include <vector>

class msaUnit_t
{
public:
  // SIMM5 of msa_vpush: [4] weight, [3:2] active-width shift, [1:0] format.
  enum fmt_t { FMT_F32 = 0, FMT_F16 = 1, FMT_E4M3 = 2, FMT_E5M2 = 3 };

  static bool is_weight(uint32_t simm5) { return (simm5 >> 4) & 1; }
  static uint32_t width_shift(uint32_t simm5) { return (simm5 >> 2) & 3; }
  static uint32_t fmt(uint32_t simm5) { return simm5 & 3; }
  static uint32_t pack(uint32_t f) { return f == FMT_F32 ? 1 : f == FMT_F16 ? 2 : 4; }

  explicit msaUnit_t(uint32_t dim) : dim(dim) { reset(); }

  void reset();
  void configure(uint32_t simm5);
  void push_weight(uint32_t col, float v);
  void push_input_row(const std::vector<float>& row);
  bool pop(uint32_t col, float& v);

  uint32_t get_dim() const { return dim; }
  uint32_t get_pack() const { return cur_pack; }
  uint32_t get_active() const { return active; }
  uint32_t depth() const { return dim * cur_pack; }

private:
  uint32_t dim;
  uint32_t cur_fmt = FMT_F32;
  uint32_t cur_pack = 1;
  uint32_t active;
  std::vector<std::deque<float>> weight;
  std::vector<std::deque<float>> out;
};

#endif
