#include "msa_unit.h"

void msaUnit_t::reset()
{
  cur_fmt = FMT_F32;
  cur_pack = 1;
  active = dim;
  weight.assign(dim, std::deque<float>());
  out.assign(dim, std::deque<float>());
}

// A weight push whose format or width differs from the loaded tile starts a new
// tile: weights packed for another K depth cannot be mixed with the old ones.
void msaUnit_t::configure(uint32_t simm5)
{
  uint32_t f = fmt(simm5);
  uint32_t a = dim >> width_shift(simm5);
  assert(a > 0);
  if (f != cur_fmt || a != active) {
    for (auto& w : weight)
      w.clear();
  }
  cur_fmt = f;
  cur_pack = pack(f);
  active = a;
}

// Each column holds the last dim*pack K-elements pushed, as the original array
// holds the last dim: the oldest falls out of a full column.
void msaUnit_t::push_weight(uint32_t col, float v)
{
  assert(col < dim);
  auto& w = weight[col];
  if (w.size() == depth())
    w.pop_front();
  w.push_back(v);
}

// One A row across all K-groups, K-ordered. Live columns produce their dot
// product; idle columns produce 0 so a pop of every lane stays deterministic.
void msaUnit_t::push_input_row(const std::vector<float>& row)
{
  assert(row.size() == depth());
  for (uint32_t j = 0; j < dim; j++) {
    float acc = 0.0f;
    if (j < active) {
      uint32_t k = 0;
      for (float w : weight[j])
        acc += row[k++] * w;
    }
    out[j].push_back(acc);
  }
}

bool msaUnit_t::pop(uint32_t col, float& v)
{
  assert(col < dim);
  if (out[col].empty())
    return false;
  v = out[col].front();
  out[col].pop_front();
  return true;
}
